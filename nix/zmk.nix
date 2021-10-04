{ stdenv, lib, buildPackages
, cmake, ninja, dtc, gcc-arm-embedded
, zephyr
, board ? "glove80_board_lh"
, shield ? "glove80_left"
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
in

stdenv.mkDerivation {
  name = "zmk_${board}_${shield}";

  sourceRoot = "source/app";

  src = builtins.path {
    name = "source";
    path = ./..;
    filter = path: type:
      let relPath = lib.removePrefix (toString ./.. + "/") (toString path);
      in (lib.cleanSourceFilter path type) && ! (
        relPath == "nix" ||
        lib.hasSuffix ".nix" path
      );
    };

  preConfigure = ''
    cmakeFlagsArray+=("-DUSER_CACHE_DIR=$TEMPDIR/.cache")
  '';

  cmakeFlags = [
    "-DZephyrBuildConfiguration_ROOT=${zephyr}/zephyr"
    # TODO: is this required? if not, why not?
    # "-DZEPHYR_BASE=${zephyr}/zephyr"
    "-DBOARD_ROOT=boards"
    "-DBOARD=${board}"
    "-DSHIELD=${shield}"
    "-DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb"
    "-DGNUARMEMB_TOOLCHAIN_PATH=${gcc-arm-embedded}"
    # TODO: maybe just use a cross environment for this gcc
    "-DCMAKE_C_COMPILER=${gcc-arm-embedded}/bin/arm-none-eabi-gcc"
    "-DCMAKE_CXX_COMPILER=${gcc-arm-embedded}/bin/arm-none-eabi-g++"
    "-DZEPHYR_MODULES=${lib.concatStringsSep ";" zephyr.modules}"
  ];

  nativeBuildInputs = [ cmake ninja python dtc ];
  buildInputs = [ zephyr ];

  installPhase = ''
    mkdir $out
    cp zephyr/zmk.uf2 $out
  '';
}