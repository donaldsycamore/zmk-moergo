require 'tmpdir'
require 'json'
require 'base64'

module LambdaFunction
  class Handler
    def self.process(event:, context:)
      ZmkCompiler.new.compile
    end
  end

  class ZmkCompiler
    def compile
      Dir.mktmpdir do |dir|
        Dir.chdir(dir)

        compile_output = nil
        IO.popen('compileZmk', err: [:child, :out]) do |io|
          compile_output = io.read
        end

        unless $?.success?
          status = $?.exitstatus
          return error_response(400, { error: "Compile failed with exit status #{status}", detail: compile_output })
        end

        unless File.exist?('zephyr/zmk.uf2')
          return error_response(500, { error: 'Compile failed to produce result binary' })
        end

        file_response(File.read('zephyr/zmk.uf2'))
      end
    rescue StandardError => e
      error_response(500, { error: 'Unexpected error', detail: e.message })
    end

    private

    def file_response(file)
      file64 = Base64.encode64(file)

      {
        'isBase64Encoded' => true,
        'statusCode' => 200,
        'body' => file64,
        'headers' => {
          'content-type' => 'application/octet-stream'
        }
      }
    end

    def error_response(code, body)
      {
        'isBase64Encoded' => false,
        'statusCode' => code,
        'body' => body.to_json,
        'headers' => {
          'content-type' => 'application/json'
        }
      }
    end
  end
end
