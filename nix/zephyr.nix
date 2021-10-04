{ stdenv, lib, fetchgit }:
let
  manifestJSON = builtins.fromJSON (builtins.readFile ./manifest-hash.json);

  projects = lib.listToAttrs (lib.forEach manifestJSON ({ name, revision, url, sha256, ... }@args: (
    lib.nameValuePair name {
      path = args.path or name;
      src = fetchgit {
        inherit name url sha256;
        rev = revision;
      };
    })
  ));
in


# Zephyr with no modules, from the frozen manifest.
# For now the modules are passed through as passthru
stdenv.mkDerivation {
  name = "zephyr";
  src = projects.zephyr.src;

  dontBuild = true;

  # This awkward structure is required by
  #   COMMAND ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/../tools/uf2/utils/uf2conv.py
  installPhase = ''
    mkdir -p $out/zephyr
    mv * $out/zephyr

    mkdir -p $out/tools/
    ln -s ${projects.uf2.src} $out/tools/uf2
  '';

  passthru = {
    # TODO: omit tools, only provide modules
    modules = map (p: p.src) (lib.attrValues (removeAttrs projects ["zephyr"]));
  };
}