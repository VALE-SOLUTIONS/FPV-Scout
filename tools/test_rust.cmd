@echo off
setlocal
rem Run from an MSVC Developer Command Prompt, with cargo on PATH.
pushd "%~dp0.."
cargo build --manifest-path espsoup\rust-scout\Cargo.toml --release --locked --offline
if errorlevel 1 goto failed
if not exist build-rust-host mkdir build-rust-host
for %%T in (scout navigation spectrum indicator composite digital remote_id) do (
 cl /nologo /std:c11 /O2 /W3 /DSCOUT_RUST_CORE /Iespsoup\tests\scout\stubs /Iespsoup\components\cyd_c5\include /Fe:build-rust-host\test_%%T.exe /Fo:build-rust-host\test_%%T.obj espsoup\tests\scout\test_%%T.c espsoup\rust-scout\target\release\fpv_scout_core.lib
 if errorlevel 1 goto failed
 build-rust-host\test_%%T.exe
 if errorlevel 1 goto failed
)
popd
exit /b 0
:failed
popd
exit /b 1
