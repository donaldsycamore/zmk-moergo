{ stdenvNoCC, lib, buildPackages
, cmake, ninja, dtc, gcc-arm-embedded
, zephyr
, board ? "glove80_lh"
, shield ? null
, keymap ? null
}:


let
  # from zephyr/scripts/requirements-base.txt
  python = buildPackages.python3.withPackages (ps: with ps; [
    pyelftools
    pyyaml
    canopen
    packaging
    progress
    anytree
    intelhex

    # TODO: this was required but not in shell.nix
    pykwalify
  ]);

  requiredZephyrModules = [
    "cmsis" "hal_nordic" "tinycrypt" "littlefs"
  ];

  zephyrModuleDeps = builtins.filter (x: builtins.elem x.name requiredZephyrModules) zephyr.modules;
in

stdenvNoCC.mkDerivation {
  name = "zmk_${board}";

  sourceRoot = "source/app";

  src = builtins.path {
    name = "source";
    path = ./..;
    filter = path: type:
      let relPath = lib.removePrefix (toString ./.. + "/") (toString path);
      in (lib.cleanSourceFilter path type) && ! (
        # Meta files
        relPath == "nix" || lib.hasSuffix ".nix" path ||
        # Transient state
        relPath == "build" || relPath == ".west" ||
        # Fetched by west
        relPath == "modules" || relPath == "tools" || relPath == "zephyr"
      );
    };

  preConfigure = ''
    cmakeFlagsArray+=("-DUSER_CACHE_DIR=$TEMPDIR/.cache")
  '';

  cmakeFlags = [
    # "-DZephyrBuildConfiguration_ROOT=${zephyr}/zephyr"
    # TODO: is this required? if not, why not?
    # "-DZEPHYR_BASE=${zephyr}/zephyr"
    "-DBOARD_ROOT=."
    "-DBOARD=${board}"
    "-DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb"
    "-DGNUARMEMB_TOOLCHAIN_PATH=${gcc-arm-embedded}"
    # TODO: maybe just use a cross environment for this gcc
    "-DCMAKE_C_COMPILER=${gcc-arm-embedded}/bin/arm-none-eabi-gcc"
    "-DCMAKE_CXX_COMPILER=${gcc-arm-embedded}/bin/arm-none-eabi-g++"
    "-DCMAKE_AR=${gcc-arm-embedded}/bin/arm-none-eabi-ar"
    "-DCMAKE_RANLIB=${gcc-arm-embedded}/bin/arm-none-eabi-ranlib"
    "-DZEPHYR_MODULES=${lib.concatStringsSep ";" zephyrModuleDeps}"
  ] ++
  (lib.optional (shield != null) "-DSHIELD=${shield}") ++
  (lib.optional (keymap != null) "-DKEYMAP_FILE=${keymap}");

  nativeBuildInputs = [ cmake ninja python dtc gcc-arm-embedded ];
  buildInputs = [ zephyr ];

  installPhase = ''
    mkdir $out
    cp zephyr/zmk.{uf2,hex,bin,elf} $out
  '';

  passthru = { inherit zephyrModuleDeps; };
}
