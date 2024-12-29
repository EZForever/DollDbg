@echo off
pushd %~dp0

if not exist "FlatBuffers\repo" (
	pushd FlatBuffers
	git clone --depth=1 --branch=v2.0.0 https://github.com/google/flatbuffers repo
	popd
)

if not exist "Detours\repo" (
	pushd Detours
	git clone --depth=1 https://github.com/Microsoft/Detours repo
	popd
)

popd