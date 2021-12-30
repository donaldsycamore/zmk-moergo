# frozen_string_literal: true

require 'tmpdir'
require 'json'
require 'base64'
require 'faraday'
require 'faraday_middleware'

module LambdaFunction
  class Handler
    SOURCE_CACHE_PATH = File.join(Dir.tmpdir, 'zmk_source')
    DEFAULT_BRANCH    = 'glove80'

    class RequestError < RuntimeError
      attr_reader :status, :detail
      def initialize(message, status: 400, detail: nil)
        super(message)
        @status = status
        @detail = detail
      end
    end

    class << self
      # ALB event structure:
      # {
      #   "requestContext": { <snip> },
      #   "httpMethod": "GET",
      #   "path": "/",
      #   "queryStringParameters": {parameters},
      #   "headers": { <snip> },
      #   "isBase64Encoded": false,
      #   "body": "request_body"
      # }
      #
      # Handle the single route: POST /compile
      def process(event:, context:)
        unless event['path'] == '/compile'
          raise RequestError.new("Unknown route: #{event['path']}", status: 404)
        end

        unless event['httpMethod'] == 'POST'
          raise RequestError.new("No route for HTTP method : #{event['httpMethod']}", status: 404)
        end

        params = event['queryStringParameters']
        keymap_data = event['body']

        raise RequestError.new('Missing POST body') unless keymap_data

        if event['isBase64Encoded']
          keymap_data = Base64.decode64(keymap_data)
        end

        revision = params.fetch('revision', DEFAULT_BRANCH)
        hand = params.fetch('hand', 'left')
        unless ['left', 'right'].include?(hand)
          raise RequestError.new('Invalid hand: #{hand}')
        end

        commit = canonicalize(revision)
        source_path = checkout(commit)
        result = compile(source_path, keymap_data, hand)

        file_response(result, commit)
      rescue RequestError => e
        error_response(status: e.status, error: e.message, detail: e.detail)
      rescue StandardError => e
        error_response(status: 500, error: "Unexpected error: #{e.class}", detail: e.message)
      end

      private

      def compile(source_path, keymap_data, hand)
        Dir.mktmpdir do |dir|
          Dir.chdir(dir)

          File.open('build.keymap', 'w') do |io|
            io.write(keymap_data)
          end

          compile_output = nil
          IO.popen(['compileZmk', source_path, './build.keymap', hand], err: [:child, :out]) do |io|
            compile_output = io.read
          end

          unless $?.success?
            status = $?.exitstatus
            raise RequestError.new("Compile failed with exit status #{status}", detail: compile_output)
          end

          unless File.exist?('zephyr/zmk.uf2')
            raise RequestError.new('Compile failed to produce result binary', status: 400)
          end

          File.read('zephyr/zmk.uf2')
        end
      end

      def checkout(commit)
        sources_path = File.join(SOURCE_CACHE_PATH, 'versions')
        locks_path   = File.join(SOURCE_CACHE_PATH, 'lock')
        lock_path    = File.join(locks_path, commit)
        source_path  = File.join(sources_path, commit)

        FileUtils.mkdir_p(sources_path)
        FileUtils.mkdir_p(locks_path)

        with_flock(lock_path) do
          return source_path if Dir.exist?(source_path)

          Dir.mkdir(source_path)

          archive_url = "https://github.com/moergo-sc/zmk/archive/#{commit}.tar.gz"

          IO.popen(['tar', '-C', source_path, '-x', '-z', '--strip-components=1'], 'wb', err: '/dev/null') do |tar_io|
            conn = Faraday.new do |f|
              f.response :follow_redirects
              f.response :raise_error
              f.adapter :net_http
            end

            response = conn.get(archive_url)
            tar_io.write(response.body)
            tar_io.close
          end

          unless $?.success?
            status = $?.exitstatus
            raise RequestError.new("Source extraction failed with exit status #{status}", status: 500)
          end

          source_path
        rescue Exception
          # Clean up source path on failure
          FileUtils.remove_entry(source_path, true)
          raise
        end
      end

      def canonicalize(revision)
        url = "https://api.github.com/repos/moergo-sc/zmk/commits/#{revision}"

        conn = Faraday.new do |f|
          f.request :retry
          f.response :json
          f.adapter :net_http
        end

        response = conn.get(url, headers: { 'Accept' => 'application/vnd.github.v3+json' })

        if response.status == 422
          raise RequestError.new("No commit found for revision '#{revision}'", status: 422)
        elsif response.status != 200 || !response.body.has_key?('sha')
          raise RequestError.new('Unexpected Github API response', status: 503, detail: response.body)
        end

        response.body['sha']
      end

      def with_flock(lockfile)
        File.open(lockfile, File::CREAT) do |f|
          f.flock(File::LOCK_EX)
          yield
        end
      end

      def file_response(file, commit)
        file64 = Base64.strict_encode64(file)

        {
          'isBase64Encoded' => true,
          'statusCode' => 200,
          'body' => file64,
          'headers' => {
            'Content-Type' => 'application/octet-stream',
            'X-Zmk-Commit' => commit,
          }
        }
      end

      def error_response(status:, error:, detail: nil)
        {
          'isBase64Encoded' => false,
          'statusCode' => status,
          'body' => { error: error, detail: detail }.to_json,
          'headers' => {
            'Content-Type' => 'application/json'
          }
        }
      end
    end
  end
end
