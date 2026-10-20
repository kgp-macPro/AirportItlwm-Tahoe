#!/bin/bash
# AirportItlwm-Tahoe 1.1.0 Beta: Desktop-only setup, adapted from KGP-supplied B.
# 2026-10-20. Project GPLv2; inherited source notices remain in enclosures.
set -euo pipefail
AIRPORT_BETA_SETUP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export AIRPORT_BETA_SETUP_DIR
exec python3 - "$@" <<'AIRPORT_BETA_PY'
PACKAGE_NAME='AirportItlwm-Tahoe-1.1.0-Beta.zip'
PACKAGE_ROOT='AirportItlwm-Tahoe-1.1.0-Beta'
RUNNER_SHA='be9c07168daf7b8ffd6f6c71ba4f62117d1e28334c4a64a9a063b2a115cc19ed'
KGP_INFO_SHA='8714070289249126ea0bfb18747fb749ce5821967ee503cd58e4496f692b02d0'
EXTERNAL_INFO_SHA='7c40b4f5357b2285ddba9d2f650d639aed13fbb2b046fb3746be98ebaf258770'
SPECS=[('ZC8', 'AirportItlwm/B1-ZC8-PERF/AirportItlwm.kext', '3677F904-4052-3E8D-8FF2-ED839F677A8B', 'c0a5ed739434fe24fd49776c5c034786bf88a6d360d8f7d7efd353c3f77472d7'), ('ZC19', 'AirportItlwm/B1-ZC19-PERF/AirportItlwm.kext', '1C2D5EBC-764D-3EF2-9714-11ADF9A2016A', '56704aaf87536e5812706ecfb97498d1fd2a09cf0b45941e74f2eb3f8e8ad624'), ('ZC22', 'AirportItlwm/B1-ZC22-PERF/AirportItlwm.kext', '2A585737-88B2-3F5C-B203-28DA9F00341D', '055bad456b53a70ba98c72e3580fcb59405c48fe09b85831a3fda1371c2a14ce'), ('ZC26', 'AirportItlwm/B1-ZC26-PERF/AirportItlwm.kext', '71D99616-428A-3D46-AFCC-9E9FD6E850AF', 'a475f204f7717d349373ccd2f2040df4401da5b1936af890ddc84ec0b5e81778'), ('ZC28', 'AirportItlwm/B1-ZC28-SERVICE/AirportItlwm.kext', '469CBE97-EEB4-3084-99C4-433E73F4FC23', 'c48b9a0a018e86f572a65166750f383f571133de5de4b9e7578e42129a1a1185'), ('LAOBAMAC53', 'Laobamac/53/AirportItlwm.kext', 'AEF5621B-B1D8-3487-BC9D-92E035BC087E', '259bac9432bffa8bf9c73411d8ac8f4a854552375e9f60ba0870ea3151ed973c')]
REFERENCES=[('ZC8_AX210_3X14_mapped_20261019_150738_kx2VLl.zip', '236dd46cbdc3e7696550f2cf47e148a5592a7ddc97c2b49eee59138617b25daa'), ('ZC19_AX210_3X14_mapped_20261018_122836_sQ4ACw.zip', '7a02e286390561b37052e42832a1ecb4e8a329a6fa310e3e830dc695c185648a'), ('ZC22_AX210_3X14_mapped_20261019_171501_x79oza.zip', 'cf4632cd88e72d840b4e58f1494ca22dc070010da2f4231b5f7dc56c8a5af547'), ('ZC26_AX210_3X14_mapped_20261020_003212_oG8Lqp.zip', '7a51d14ce063f924a1bd830cd32a0beaec683b12664cf0b7287da0e31e2600c6'), ('ZC28_AX210_3X14_mapped_20261020_145825_7drjMn.zip', '69edc360887db5a53274b69de884a632110b6d1303464e8837e11ab31a2b9678'), ('ZC28_AX210_3X14_conventional_20261020_164527_EJOvrp.zip', '6b4a3c9c70f6327576f6b7e2d7788ed9aeac0db312a169efd1877539f8bad238'), ('LAOBAMAC_53_738D4C6_MAPPED_AX210_3X14_unverified_20261020_122534_n2sdvy.zip', 'c4ce44cacc429bafb7b362e25034028529c044695c366ed1c55f0147837bef70')]
ENCLOSURE_HASHES={'AirportItlwm/SOURCES/BETA_DOCUMENTATION/CREDITS.md': 'e1d3a16aa12b1e3b3f0ffb4895b631df17f7eb87f61043751171430bad00a185', 'AirportItlwm/SOURCES/BETA_DOCUMENTATION/FORUM_CAMPAIGN_MEANS_VALIDATION.json': '239f13d48931d104bf1a614c006e174fa5b1363d745663ed81a29e02c6d67621', 'AirportItlwm/SOURCES/BETA_DOCUMENTATION/HARDWARE_COVERAGE.md': 'a19caf51f1d1f083bd4b23b6f73abe6524b3d662819ef8b805aabf6c8db7d83a', 'AirportItlwm/SOURCES/BETA_DOCUMENTATION/HARDWARE_SOURCE_LEDGER.json': 'e3dd2cb0a076f09be16e1408a0d7b47586c154b4432f439d3a20cd46ae3b9ffa', 'AirportItlwm/SOURCES/FRAMEWORK_ENVIRONMENT_REGISTER.json': '50211b0c57cbc38620ddc07edc9db375b92f3408c381bc2ba31b1d0362d0f04b', 'AirportItlwm/SOURCES/FRAMEWORK_ENVIRONMENT_REGISTER.txt': '8ce8def253575f0f1c23efe45ff1cc05dbecb87710f76edca7901df193782807', 'AirportItlwm/SOURCES/LAOBAMAC53_PROVENANCE/EXTERNAL_FETCH_RECORD.json': '1cff6518682bb10c7144361aa3811f1a19e779e051f686e689360e55a12d2ff1', 'AirportItlwm/SOURCES/LAOBAMAC53_PROVENANCE/FETCH_RECORD.json': 'a3638267160edf0ab1ef7150bd93a204e7515bec721fe7bcb3f665c7d280be54', 'AirportItlwm/SOURCES/LAOBAMAC53_PROVENANCE/laobamac-commit.json': 'a055b0dea1d334c9219c08bf93619ad8df00c6c609e0f7ab6a226548353d9505', 'AirportItlwm/SOURCES/LAOBAMAC53_PROVENANCE/laobamac-releases.json': 'db59f755b5b0235ab4bff1e69caef88d8a6faf4ff99e984662795fecea83f6bf', 'AirportItlwm/SOURCES/LAOBAMAC53_PROVENANCE/laobamac-runs.json': '6d29dd99d98e125befb39586b2c0a70ca3873a040f23c17ea7e71080e9d3efe9', 'AirportItlwm/SOURCES/LAOBAMAC53_PROVENANCE/run53-artifacts.json': '9ee79ea79de298e87fcfea39a0137a1b55bbca6613b9e8d85f59a605f33093c7', 'AirportItlwm/SOURCES/LAOBAMAC53_PROVENANCE/run53-jobs.json': '8d1ec815d9318f23770a456f52c26c9843410703866a5e7b2ac44b9688c2be04', 'AirportItlwm/SOURCES/LAOBAMAC53_PROVENANCE/run53-workflow.yml': '5433fdc3d3f0bbf3dd1c932c66e923bda46eadf8e45973701946f04fbd04eba6', 'AirportItlwm/LICENSES/LAOBAMAC53/LICENSE': '8177f97513213526df2cf6184d8ff986c675afb514d4e68a404010521b880643', 'AirportItlwm/LICENSES/LAOBAMAC53/MacKernelSDK-LICENSE.txt': 'e5881019d8766c1e88a5fe1dbca4ba40c78011d41fcb18f6e9f50df60182685b', 'AirportItlwm/LICENSES/LAOBAMAC53/iwlwifi-firmware-license': '16d5040c7cf851fc693b7542e20870935b99802533ea1bfd231c377a2305e5c5', 'AirportItlwm/LICENSES/ZC19/LICENSE': '8177f97513213526df2cf6184d8ff986c675afb514d4e68a404010521b880643', 'AirportItlwm/LICENSES/ZC19/MacKernelSDK/LICENSE.txt': 'e5881019d8766c1e88a5fe1dbca4ba40c78011d41fcb18f6e9f50df60182685b', 'AirportItlwm/LICENSES/ZC19/iwlwifi-firmware-license': '16d5040c7cf851fc693b7542e20870935b99802533ea1bfd231c377a2305e5c5', 'AirportItlwm/LICENSES/ZC22/LICENSE': '8177f97513213526df2cf6184d8ff986c675afb514d4e68a404010521b880643', 'AirportItlwm/LICENSES/ZC22/MacKernelSDK/LICENSE.txt': 'e5881019d8766c1e88a5fe1dbca4ba40c78011d41fcb18f6e9f50df60182685b', 'AirportItlwm/LICENSES/ZC22/iwlwifi-firmware-license': '16d5040c7cf851fc693b7542e20870935b99802533ea1bfd231c377a2305e5c5', 'AirportItlwm/LICENSES/ZC26/LICENSE': '8177f97513213526df2cf6184d8ff986c675afb514d4e68a404010521b880643', 'AirportItlwm/LICENSES/ZC26/MacKernelSDK/LICENSE.txt': 'e5881019d8766c1e88a5fe1dbca4ba40c78011d41fcb18f6e9f50df60182685b', 'AirportItlwm/LICENSES/ZC26/iwlwifi-firmware-license': '16d5040c7cf851fc693b7542e20870935b99802533ea1bfd231c377a2305e5c5', 'AirportItlwm/LICENSES/ZC28/LICENSE': '8177f97513213526df2cf6184d8ff986c675afb514d4e68a404010521b880643', 'AirportItlwm/LICENSES/ZC28/MacKernelSDK/LICENSE.txt': 'e5881019d8766c1e88a5fe1dbca4ba40c78011d41fcb18f6e9f50df60182685b', 'AirportItlwm/LICENSES/ZC28/iwlwifi-firmware-license': '16d5040c7cf851fc693b7542e20870935b99802533ea1bfd231c377a2305e5c5', 'AirportItlwm/LICENSES/ZC8/LICENSE': '8177f97513213526df2cf6184d8ff986c675afb514d4e68a404010521b880643', 'AirportItlwm/LICENSES/ZC8/MacKernelSDK/LICENSE.txt': 'e5881019d8766c1e88a5fe1dbca4ba40c78011d41fcb18f6e9f50df60182685b', 'AirportItlwm/LICENSES/ZC8/iwlwifi-firmware-license': '16d5040c7cf851fc693b7542e20870935b99802533ea1bfd231c377a2305e5c5', 'AirportItlwm/SOURCES/MacKernelSDK-3f750085caa17ec3a7880f11c11bf4f48cd6a164.tar.gz': '8931e622b6d16a7052cb2c2dabe16be06963a34b747f19e6cda2bbe8bc7e2557', 'AirportItlwm/SOURCES/README_BEST_RESULT_SELECTION.json': 'f093ccb291969c7a50fd6b93ad243c3e5ab33753288b1781ddc43e61c2877d15', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/LAOBAMAC_53_738D4C6_CONVENTIONAL_AX210_3X14_unverified_20261020_123708_tPD1WD.zip': 'f7971fbe28d70c826c40493a451d51fdc38b9524591f4a9c4bad7ffbcfd0f4ba', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC19_AX210_3X14_mapped_20261018_124218_w7JoER.zip': '87c63eb9fd81753cff4572a89466fae8268b0680829522503c1dde5def676cc8', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC22_AX210_3X14_conventional_20261020_104551_E5jO9B.zip': '0a32b003ab46bd94bb2643c97eecc5d4444248776f595b247006b83a43a97c25', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC22_AX210_3X14_mapped_20261019_164056_Jkx7Fy.zip': 'abdf6a47ecca104fd8a3b17b4cd9f2744672d0162a7a85adf72af47de6d69fa9', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC22_AX210_3X14_mapped_20261019_165553_mg0zW6.zip': '525488c21e681dca5a478a416a3a169ff44a7b4889865dc4b6ba5435f6928d13', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC22_AX210_3X14_mapped_20261020_103118_D2jqOx.zip': '879f16854f744d5eda2769ba03089d1dcd8eb68ecb1f92b04e349d83eee21812', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_conventional_20261020_005919_gYhHiF.zip': '1bd699045d1bb46957fee17d0f0a436e96ec0fb68f4641d4a222dc50c5fb3db1', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_conventional_20261020_010624_GjpQ6c.zip': '84eb711412db52cb8a24b1e44403c311c12662f9ba350191ba7ab4ea47d31a30', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_conventional_20261020_011439_shm8ZC.zip': 'f647f654052e739602f23a13ddcc91bae7658dd54a616578af01104d268d5f78', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_conventional_20261020_020338_9q3Qg9.zip': '4e1b5b0b2fb9384011652f8155a4634c5248e41a2b048a9daf1b4b55d6c16b62', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_conventional_20261020_021647_0AqQaa.zip': 'e946cb4a1fc12441ea2ec0de44dd75dbae165b6d9ff26466c77111740d27537c', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_conventional_20261020_091811_BGiPKz.zip': 'd2ebf38b91af081badaa5959cb0bf3da978f8844343d1fe6d0357692c2334932', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_conventional_20261020_095351_FdAI70.zip': '252ae26910a0e84351d5ff07215cf92d991ea61f1e351821aa17bbb580f99763', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_conventional_20261020_100133_TKv0oy.zip': '2bd7811149fba84d13c1dc8db77867d4020208839feaab21c8c597266990107d', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_conventional_20261020_101357_L0KO0B.zip': '7caada28480f307f264d271e53a01f96aee6c1010c9a5be07b6ab2b42858f060', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_mapped_20261020_002422_nwtUQN.zip': '7ee0c244177e43b65245723dbcdaf70d2fc4f13d879d69247abd39b6b9b5b930', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_mapped_20261020_003906_qkntNm.zip': '560c3480674d824f9c26190fdb5ac880602cfae22a341c86489eccf7baf44f9e', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC26_AX210_3X14_mapped_20261020_110027_MfOLxX.zip': '4d4d4eaa9ad1250d9d19ea06f57c82ad26e9c29906330125633585c0a2848cfb', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC28_AX210_3X14_conventional_20261020_165222_nUUFOr.zip': '8ce7424e8f45f1542e1c926130c7153c1d826ccd2be5a645bf8d30ee344a2f27', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC28_AX210_3X14_conventional_20261020_170008_QsHMcG.zip': 'ecb2213efe37eca380a6abe5e5727622a24e1c5728cc3c57991205cc1e9c5396', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC28_AX210_3X14_mapped_20261020_144057_7zXbzs.zip': '39590586587f505c75919933616c10b2a1d83721daea6bb9a5697dc55b992e16', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC28_AX210_3X14_mapped_20261020_144921_5lIpTl.zip': '01bdf430d8689e9e3f33ae19453d80ac5aea26e1180b3676496d67af6a7c7fec', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC8_AX210_3X14_conventional_20261018_032335_brBhDj.zip': '6dcff0be32bd4f7af5c0364933641a71ae2ea9dd80f5eaddc966013ce9b781f3', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC8_AX210_3X14_conventional_20261020_113857_n0Cnrb.zip': 'c98cc06b9e3e923ffb63e1aa3ebabd81f77f597058b65b5147a02a1f4963166b', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC8_AX210_3X14_mapped_20261018_025805_S2ZwkD.zip': '2abbe45d9977b9326b19ce9eb09c3eff25b6b5875c678833d341e0733fa83586', 'AirportItlwm/SOURCES/README_TABLE_ORIGINS/ZC8_AX210_3X14_mapped_20261020_112715_nRtmGO.zip': '20fc009cc99ba7eb2dba346200b274f6c5002de327fdeb5fe423fae11d453646', 'AirportItlwm/SOURCES/SOURCE_BINARY_LICENSE_MAP.json': 'da6650805aa75ad0eb44da8117721d19bdd209662d35d88e4966db0e7cc4d738', 'AirportItlwm/SOURCES/SOURCE_BUILD_README.txt': 'c32687ed6feb3cf137ac84aedfd99a505460752307b6baf98b0bfccde2cad340', 'AirportItlwm/SOURCES/SOURCE_LICENSE_MAP.txt': 'e480792ad625dc4ba316a310774ce4cf5c384f810380424aaa0cd27a2f9fc181', 'AirportItlwm/SOURCES/ZC19_BUILD_RECORDS/BUILD_COMMAND.json': '6a43c098dc8e8c5adffab90cd460f4bf5487adade8fbcbebca5507ceb83b5617', 'AirportItlwm/SOURCES/ZC19_BUILD_RECORDS/BUILD_INPUT.json': 'd90a884f23bb8713ea05e9917b80cca411877f9c61c741ade9e6d65e855383d5', 'AirportItlwm/SOURCES/ZC19_BUILD_RECORDS/BUILD_RESULT.json': '961670433a8d216eb16d7bc6a340e1a172e39ef14566311422025123d4762d15', 'AirportItlwm/SOURCES/ZC19_BUILD_RECORDS/BUILD_SOURCE_CONSISTENCY.json': '2d44151a277d85ac869c2530b3f7682653d8075b8da18abccaadd22d4ed44236', 'AirportItlwm/SOURCES/ZC19_BUILD_RECORDS/SOURCE_SHA256SUMS.txt': 'e18bf661da94e643bfba1ebd80d14895d4960f7d49954625bdfe852c7130ced2', 'AirportItlwm/SOURCES/ZC19_BUILD_RECORDS/TOOLCHAIN.json': '0756aaeaa2f2ac76f18a297d38c71764ea299d7bf44202cc820462df33a91e7f', 'AirportItlwm/SOURCES/ZC19_CORRESPONDING_SOURCE_f18801f700b4.tar.gz': 'b6eac623aa5955b835406a46053e28c5fcb67e2fb0d88daa993c68d9acc93709', 'AirportItlwm/SOURCES/ZC22_BUILD_RECORDS/BUILD_COMMAND.json': 'b004253aabe538d46d12797ec3cf88b2b00651873c22f6b418a9f55d1f3e23d2', 'AirportItlwm/SOURCES/ZC22_BUILD_RECORDS/BUILD_INPUT.json': '5ee50271f94951abdf156a9139eaaa937dfa77373839c757fa65bdde41b2e3e6', 'AirportItlwm/SOURCES/ZC22_BUILD_RECORDS/BUILD_RESULT.json': 'cb071d5fb8911435e7f2554871a33c3371bdd712c2cd38d065da65d6f7166332', 'AirportItlwm/SOURCES/ZC22_BUILD_RECORDS/BUILD_SOURCE_CONSISTENCY.json': 'ec81addf90254c08fe24220df44723418e2c23dafe9ff1281b23b52a982786cc', 'AirportItlwm/SOURCES/ZC22_BUILD_RECORDS/SOURCE_SHA256SUMS.txt': 'accdb7954632744181f2d95245689a1061532221ea2d868da25d47ed318c2e3f', 'AirportItlwm/SOURCES/ZC22_BUILD_RECORDS/TOOLCHAIN.json': '1b0a11c8570e97b6fe946f4fa327998633345b98ae90336fbbbe9ac8660bc7b8', 'AirportItlwm/SOURCES/ZC22_CORRESPONDING_SOURCE_7dd260fbfb9d.tar.gz': 'f2831033ec8f8a330eb7dc7b2dccfe104c14d5d2e8cb026fedced711555442bb', 'AirportItlwm/SOURCES/ZC26_BUILD_RECORDS/BUILD_COMMAND.json': 'b57f600f29effe14196a79a78235ecf6517927c2b83246941e321a5017b81c4c', 'AirportItlwm/SOURCES/ZC26_BUILD_RECORDS/BUILD_INPUT.json': '68f770c266f946d20ceeb8f317e97e57f3e9478788701f4e9c688f5a443c033d', 'AirportItlwm/SOURCES/ZC26_BUILD_RECORDS/BUILD_RESULT.json': 'a76c878f55136229cbae252b47c965fb4d2415ca50b2e86e09cde69f4fc01865', 'AirportItlwm/SOURCES/ZC26_BUILD_RECORDS/BUILD_SOURCE_CONSISTENCY.json': '163349960d09906b4fda6c722d28f23e5b9a34b391196b9e214262d4b5d57c63', 'AirportItlwm/SOURCES/ZC26_BUILD_RECORDS/SOURCE_SHA256SUMS.txt': '9b9064002e213492cc7d2e2e544f93ae211529a8b94058e541536c71c2002784', 'AirportItlwm/SOURCES/ZC26_BUILD_RECORDS/TOOLCHAIN.json': 'a1ad58766fc39ab7aaba10f80323791347a4fb88bc3996a18e62fb2f134b4bb3', 'AirportItlwm/SOURCES/ZC26_CORRESPONDING_SOURCE_48e9a31e68c3.tar.gz': '0eb1590afc8b651e7f0a068ddbf5aba3ba8fa6c12604d04a076de8265fc6b0a3', 'AirportItlwm/SOURCES/ZC28_BUILD_RECORDS/BUILD_COMMAND.json': 'f7f20489372f678a143715613d3dda82f05d97461db6a4eb48a38345b46486f5', 'AirportItlwm/SOURCES/ZC28_BUILD_RECORDS/BUILD_INPUT.json': '79bf02fed55706e062f6b06fd499819160ade067c873ee1dfa807adf172d16fd', 'AirportItlwm/SOURCES/ZC28_BUILD_RECORDS/BUILD_RESULT.json': '01e3f47bb2cf0f9a4d91c90fc52bf368e6db0a0cf13985e04258ccde6fabd7df', 'AirportItlwm/SOURCES/ZC28_BUILD_RECORDS/BUILD_SOURCE_CONSISTENCY.json': 'aecdfd1716d70ecce8f3b31798f096fba517399e1d57b153fefb90beeefca3a3', 'AirportItlwm/SOURCES/ZC28_BUILD_RECORDS/SOURCE_SHA256SUMS.txt': 'd1b6b283a14cd4ea7564bb0dc99ecb3bb85d0208f4f53e1b6e63f1e8061c821b', 'AirportItlwm/SOURCES/ZC28_BUILD_RECORDS/TOOLCHAIN.json': '353b0d2a126712f3d2df61bff3e90348bece3583a0f670d29b0b500772455bb0', 'AirportItlwm/SOURCES/ZC28_CORRESPONDING_SOURCE_483940988c50.tar.gz': 'e074dec31ad7b87d5d0ce5eac7685ca65e39b50c4f5fdb51f413659f4a5ef9a9', 'AirportItlwm/SOURCES/ZC8_BUILD_RECORDS/BUILD_COMMAND.json': 'da7f877fb18f6836224b1e3f7a41b1530908ce8bdba5aac2c88c1262729d2042', 'AirportItlwm/SOURCES/ZC8_BUILD_RECORDS/BUILD_INPUT.json': 'e93ed0aba4826a9835015a930c4f6c032a995597cfb6123cc900272770e71bb8', 'AirportItlwm/SOURCES/ZC8_BUILD_RECORDS/BUILD_RESULT.json': 'b9ff61ce98d8fb095e950eb93b824b1bce50f7587dd531b63e8cf06714547530', 'AirportItlwm/SOURCES/ZC8_BUILD_RECORDS/BUILD_SOURCE_CONSISTENCY.json': '7ca90d75e2979d9b8b37da2dca42804f3e35c7d85511ff8396c6633119345c89', 'AirportItlwm/SOURCES/ZC8_BUILD_RECORDS/SOURCE_SHA256SUMS.txt': '12503c1f6827f065debcfd61abf1f10c3b4de58a761f076f5232eba38db373e0', 'AirportItlwm/SOURCES/ZC8_BUILD_RECORDS/TOOLCHAIN.json': 'eebfd82fd5a2c76a8ab506ece05cb3690e236f2ccd03e70334aabf425fa2239d', 'AirportItlwm/SOURCES/ZC8_CORRESPONDING_SOURCE_db22bb90b72b.tar.gz': 'd6b69e060e50cba2ef2e7ccf8b44d1a2dbb01e47abc57356bd86096177a67bb5', 'AirportItlwm/SOURCES/laobamac53-source.tar.gz': 'a0f036eb462e13d7995781569acf266d1683f5a3c55f916a1c3d2d49895d4c73'}
def macho_uuid(data):
    # The recorded executables are x86_64 Mach-O. Also accept a universal container.
    if data[:4] in (b'\xca\xfe\xba\xbe',b'\xca\xfe\xba\xbf'):
        if len(data)<8:raise ValueError('Truncated universal Mach-O.')
        count=struct.unpack_from('>I',data,4)[0];wide=data[:4]==b'\xca\xfe\xba\xbf';width=32 if wide else 20
        if count>128 or 8+count*width>len(data):raise ValueError('Malformed universal Mach-O.')
        matches=[]
        for i in range(count):
            fields=struct.unpack_from('>iiQQII' if wide else '>iiIII',data,8+i*width)
            cpu,offset,size=fields[0],fields[2],fields[3]
            if offset<8+count*width or offset+size>len(data):raise ValueError('Invalid universal slice bounds.')
            if cpu==0x01000007:matches.append(data[offset:offset+size])
        if len(matches)!=1:raise ValueError('Expected one x86_64 Mach-O slice.')
        return macho_uuid(matches[0])
    if len(data)<32 or data[:4]!=b'\xcf\xfa\xed\xfe':raise ValueError('Expected little-endian 64-bit Mach-O.')
    magic,cpu,subtype,filetype,ncmds,sizeofcmds,flags,reserved=struct.unpack_from('<IiiIIIII',data)
    if cpu!=0x01000007 or ncmds>10000 or 32+sizeofcmds>len(data):raise ValueError('Invalid x86_64 Mach-O header.')
    pos=32;found=[]
    for _ in range(ncmds):
        if pos+8>32+sizeofcmds:raise ValueError('Truncated Mach-O command.')
        cmd,size=struct.unpack_from('<II',data,pos)
        if size<8 or pos+size>32+sizeofcmds:raise ValueError('Invalid Mach-O command bounds.')
        if cmd==0x1b:
            if size!=24:raise ValueError('Invalid LC_UUID size.')
            found.append(str(uuid.UUID(bytes=data[pos+8:pos+24])).upper())
        pos+=size
    if pos!=32+sizeofcmds or len(found)!=1:raise ValueError('Expected exactly one Mach-O UUID.')
    return found[0]
from pathlib import Path,PurePosixPath
import datetime,hashlib,json,os,shutil,stat,sys,tempfile,zipfile,subprocess,struct,uuid

# Constants and exact binary/source pins are generated above from the frozen
# review inputs. This is Desktop setup only; no EFI or driver loading action.
BUNDLES=[s[1] for s in SPECS]
SINGLES=['AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh','AirportItlwm/README_TESTS.txt','AirportItlwm/REFERENCE_NOTES.txt','AirportItlwm/ZC28_CONVENTIONAL_ANALYSIS.txt']
ROOT_FILES={'SETUP_AirportItlwm_Beta.command','README_START_HERE.txt','SHA256SUMS.txt'}
def digest(data):return hashlib.sha256(data).hexdigest()
def canonical(name):
 p=PurePosixPath(name)
 if not name or p.is_absolute() or '..' in p.parts or '\\' in name or p.as_posix()!=name:raise ValueError('Unsafe/noncanonical package path: '+name)
 return p
def reference_file(name):
 p=PurePosixPath(name);return len(p.parts)==3 and p.parts[:2]==('AirportItlwm','REFERENCE_RESULTS') and p.suffix=='.zip'
def allowed_file(name):
 return name in ROOT_FILES or name in SINGLES or reference_file(name) or name in ENCLOSURE_HASHES or any(name.startswith(b+'/') for b in BUNDLES)
def tree_signature(path):
 if path.is_symlink():raise ValueError('Destination contains a symbolic link: '+str(path))
 if path.is_file():return {'':(digest(path.read_bytes()),stat.S_IMODE(path.stat().st_mode)&0o111)}
 if not path.is_dir():raise ValueError('Unsupported destination item: '+str(path))
 out={}
 for p in sorted(path.rglob('*')):
  if p.is_symlink():raise ValueError('Destination contains a symbolic link: '+str(p))
  if p.is_file():out[p.relative_to(path).as_posix()]=(digest(p.read_bytes()),stat.S_IMODE(p.stat().st_mode)&0o111)
  elif not p.is_dir():raise ValueError('Unsupported destination item: '+str(p))
 return out
def no_link_parents(desktop,relative):
 current=desktop
 for part in PurePosixPath(relative).parts:
  current=current/part
  if current.is_symlink():raise ValueError('Destination is a symbolic link; no changes made: '+str(current))
  if current.exists() and current!=desktop/relative and not current.is_dir():raise ValueError('Destination parent is not a directory: '+str(current))
def validate_contents(folder,manifest):
 if not isinstance(manifest,dict):raise ValueError('Manifest is not an object.')
 if manifest.get('schema')!='AIRPORTITLWM_BETA_PACKAGE_V2' or manifest.get('public_name')!='AirportItlwm-Tahoe 1.1.0 Beta':raise ValueError('Wrong package schema/public identity.')
 expected=manifest.get('files')
 if not isinstance(expected,dict) or not ROOT_FILES.issubset(expected):raise ValueError('Missing manifest/setup/start/checksum files.')
 for n in expected:
  canonical(n)
  if not allowed_file(n):raise ValueError('Unexpected package destination: '+n)
 for n in SINGLES:
  if n not in expected:raise ValueError('Required file absent: '+n)
 if set(n for n in expected if reference_file(n))!={'AirportItlwm/REFERENCE_RESULTS/'+n for n,h in REFERENCES}:raise ValueError('Expected exactly seven pinned original reference ZIPs.')
 for n,h in ENCLOSURE_HASHES.items():
  if n not in expected or digest((folder/n).read_bytes())!=h:raise ValueError('Corresponding source/license input absent or changed: '+n)
 for n,entry in expected.items():
  p=folder/n
  if p.is_symlink() or not p.is_file():raise ValueError('Missing/nonregular payload: '+n)
  data=p.read_bytes();mode=stat.S_IMODE(p.stat().st_mode)&0o777
  if not isinstance(entry,dict) or len(data)!=entry.get('bytes') or digest(data)!=entry.get('sha256') or mode!=entry.get('mode'):raise ValueError('Package checksum/size/permissions mismatch: '+n)
 for label,rel,want_uuid,want_sha in SPECS:
  exe=folder/rel/'Contents/MacOS/AirportItlwm';info=folder/rel/'Contents/Info.plist'
  if digest(exe.read_bytes())!=want_sha or macho_uuid(exe.read_bytes())!=want_uuid:raise ValueError('Wrong frozen binary identity: '+label)
  if digest(info.read_bytes())!=(EXTERNAL_INFO_SHA if label=='LAOBAMAC53' else KGP_INFO_SHA):raise ValueError('Wrong frozen Info.plist: '+label)
  if not exe.stat().st_mode&0o111:raise ValueError('Nonexecutable candidate: '+label)
 for n,h in REFERENCES:
  if digest((folder/'AirportItlwm/REFERENCE_RESULTS'/n).read_bytes())!=h:raise ValueError('Reference original changed: '+n)
 if digest((folder/SINGLES[0]).read_bytes())!=RUNNER_SHA:raise ValueError('Benchmark differs from unchanged physically used r7.')
 return BUNDLES+SINGLES+['AirportItlwm/SOURCES','AirportItlwm/LICENSES']+sorted(n for n in expected if reference_file(n)),manifest

def verify_and_extract(archive,folder):
 with zipfile.ZipFile(archive) as z:
  infos=z.infolist();names=[i.filename for i in infos]
  if len(names)!=len(set(names)):raise ValueError('Duplicate ZIP members.')
  if sum(i.file_size for i in infos)>2*1024**3:raise ValueError('Unexpectedly large beta package.')
  for i in infos:
   canonical(i.filename.rstrip('/') if i.is_dir() else i.filename)
   if not i.filename.startswith(PACKAGE_ROOT+'/'):raise ValueError('Wrong top-level package directory.')
   kind=stat.S_IFMT(i.external_attr>>16)
   if kind not in (0,stat.S_IFREG,stat.S_IFDIR):raise ValueError('Links/special files not accepted: '+i.filename)
  manifest=json.loads(z.read(PACKAGE_ROOT+'/PACKAGE_MANIFEST.json'))
  if not isinstance(manifest,dict):raise ValueError('Manifest is not an object.')
  expected=manifest.get('files',{})
  if {n[len(PACKAGE_ROOT)+1:] for n in names if not n.endswith('/')}!=set(expected)|{'PACKAGE_MANIFEST.json'}:raise ValueError('ZIP/manifest file coverage differs.')
  for i in infos:
   n=i.filename[len(PACKAGE_ROOT)+1:]
   if not n:continue
   if i.is_dir():
    if not(n.rstrip('/')=='AirportItlwm/TESTS' or any(p.startswith(n.rstrip('/')+'/') for p in expected)):raise ValueError('Unexpected package directory: '+n)
    continue
   p=folder/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(z.read(i.filename));os.chmod(p,stat.S_IMODE(i.external_attr>>16)&0o777 or 0o644)
 return validate_contents(folder,manifest)

def verify_directory(source,folder):
 if source.is_symlink() or not source.is_dir():raise ValueError('Extracted package directory is missing or symbolic.')
 m=source/'PACKAGE_MANIFEST.json'
 if m.is_symlink():raise ValueError('Manifest is symbolic.')
 manifest=json.loads(m.read_text())
 if not isinstance(manifest,dict):raise ValueError('Manifest is not an object.')
 expected=manifest.get('files',{});seen=set()
 for p in source.rglob('*'):
  if p.is_symlink():raise ValueError('Extracted package contains symbolic link: '+str(p))
  if p.is_file():
   n=p.relative_to(source).as_posix()
   if p.name=='.DS_Store':continue # Finder metadata, never installed/copied.
   canonical(n);seen.add(n)
  elif not p.is_dir():raise ValueError('Extracted package has special item.')
 if seen!=set(expected)|{'PACKAGE_MANIFEST.json'}:raise ValueError('Extracted folder/manifest coverage differs.')
 # Validate BEFORE copying; only declared managed payload enters temporary stage.
 units,manifest=validate_contents(source,manifest)
 for n in expected:
  p=folder/n;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/n,p)
 return units,manifest

def setup(archive,desktop):
 archive=Path(archive).expanduser();desktop=Path(desktop).expanduser()
 if archive.is_symlink() or desktop.is_symlink():raise ValueError('Package/Desktop may not be a symbolic link.')
 archive=archive.resolve();desktop=desktop.resolve()
 if not desktop.is_dir():raise ValueError('Desktop directory not found: '+str(desktop))
 with tempfile.TemporaryDirectory(prefix='airport_beta_verify_') as tmp:
  extracted=Path(tmp)
  units,manifest=verify_directory(archive,extracted) if archive.is_dir() else verify_and_extract(archive,extracted)
  for unit in units:no_link_parents(desktop,unit)
  for name in ('AirportItlwm/TESTS','AirportItlwm/TOOLS','Laobamac/53'):
   no_link_parents(desktop,name);target=desktop/name
   if target.exists() and not target.is_dir():raise ValueError('Required directory is a file: '+str(target))
  changed=[];skipped=[]
  for unit in units:
   target=desktop/unit
   if target.exists() and tree_signature(target)==tree_signature(extracted/unit):skipped.append(unit)
   else:changed.append(unit)
  backup=desktop/('AirportItlwm_Beta_Backup_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));operations=[]
  with tempfile.TemporaryDirectory(prefix='.airport_beta_stage_',dir=desktop) as stage_tmp:
   stage=Path(stage_tmp)
   for unit in changed:
    src=extracted/unit;dst=stage/unit;dst.parent.mkdir(parents=True,exist_ok=True)
    if src.is_dir():shutil.copytree(src,dst)
    else:shutil.copy2(src,dst)
    if tree_signature(src)!=tree_signature(dst):raise ValueError('Staged copy differs: '+unit)
   try:
    for unit in changed:
     dst=desktop/unit;old=backup/unit;dst.parent.mkdir(parents=True,exist_ok=True)
     record={'destination':dst,'backup':None,'installed':False};operations.append(record)
     if dst.exists():old.parent.mkdir(parents=True,exist_ok=True);os.replace(dst,old);record['backup']=old
     os.replace(stage/unit,dst);record['installed']=True
    for name in ('AirportItlwm/TESTS','AirportItlwm/TOOLS','Laobamac/53'):(desktop/name).mkdir(parents=True,exist_ok=True)
   except BaseException:
    for rec in reversed(operations):
     dst=rec['destination']
     if rec['installed']:
      if dst.is_dir():shutil.rmtree(dst)
      elif dst.exists():dst.unlink()
     if rec['backup'] is not None:os.replace(rec['backup'],dst)
    raise
  return {'installed_or_updated':changed,'already_identical':skipped,'backup':str(backup) if backup.exists() else None,'readme':str(desktop/'AirportItlwm/README_TESTS.txt'),'runner':str(desktop/SINGLES[0]),'runner_sha256':RUNNER_SHA}

def find_archive():
 if len(sys.argv)>2:raise ValueError('Usage: SETUP_AirportItlwm_Beta.command [extracted-folder-or-ZIP]')
 if len(sys.argv)==2:return Path(sys.argv[1])
 folder=Path(os.environ.get('AIRPORT_BETA_SETUP_DIR',''))
 if (folder/'PACKAGE_MANIFEST.json').is_file():return folder
 search=[folder,folder.parent,Path.home()/'Downloads'];found=[]
 for f in search:
  p=f/PACKAGE_NAME
  if p.is_file() and p.resolve() not in found:found.append(p.resolve())
 if len(found)!=1:raise ValueError('Run setup from the extracted '+PACKAGE_ROOT+' folder, or pass the exact package path.')
 return found[0]
def open_instructions(path):
 # The test switch suppresses GUI ONLY in the local temporary-home test harness.
 # It does not suppress validation, copy, pins, backups or touch the benchmark.
 if os.environ.get('AIRPORT_BETA_TEST_NO_OPEN')=='1':return
 try:
  p=subprocess.run(['/usr/bin/open','-t',str(path)],check=False)
  if p.returncode:print('Open the quick start manually: '+str(path))
 except OSError:print('Open the quick start manually: '+str(path))
def main():
 r=setup(find_archive(),Path.home()/'Desktop')
 print('AirportItlwm-Tahoe 1.1.0 Beta: Desktop setup complete; EFI/installed driver unchanged.')
 print('Updated: '+str(len(r['installed_or_updated']))+'; identical: '+str(len(r['already_identical'])))
 if r['backup']:print('Previous differing DESKTOP files preserved: '+r['backup'])
 print('OPEN QUICK START: '+r['readme']);open_instructions(r['readme'])
if __name__=='__main__':
 try:main()
 except (ValueError,OSError,KeyError,zipfile.BadZipFile,json.JSONDecodeError) as e:raise SystemExit('STOP: '+str(e))

AIRPORT_BETA_PY
