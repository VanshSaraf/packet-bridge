# PacketBridge Testing

PacketBridge includes lightweight assert-based C++ tests plus manual smoke tests for local networking workflows.

## Build

```sh
cmake -S . -B build
cmake --build build
```

If CMake is unavailable, build with your platform compiler or IDE using C++17 and the sources listed in `CMakeLists.txt`.

## Automated Tests

The CMake build creates `packetbridge_tests`.

```sh
ctest --test-dir build --output-on-failure
```

The tests cover:

- SHA-256 known vector for `abc`
- filename sanitization
- file manifest serialization
- chunk header serialization
- hash packet serialization
- continuation request serialization
- checkpoint save/load behavior

## CI

The GitHub Actions workflow in `.github/workflows/ci.yml` configures CMake on Ubuntu, builds all targets, and runs `ctest`.

## Discovery Smoke Test

Terminal 1:

```sh
./build/packetbridge_listener
```

Terminal 2:

```sh
./build/packetbridge_broadcaster test-node
```

Expected result: the listener prints `test-node` in the peer table.

## Text File Transfer

Create a sample:

```sh
printf 'hello from PacketBridge\n' > /tmp/pb-text.txt
mkdir -p /tmp/pb-out
```

Terminal 1:

```sh
./build/packetbridge_receiver /tmp/pb-out
```

Terminal 2:

```sh
./build/packetbridge_sender 127.0.0.1 /tmp/pb-text.txt
```

Verify:

```sh
cmp /tmp/pb-text.txt /tmp/pb-out/received_pb-text.txt
```

## Binary File Transfer

Create a multi-chunk file:

```sh
head -c 262144 /dev/urandom > /tmp/pb-binary.bin
mkdir -p /tmp/pb-out
```

Run receiver and sender:

```sh
./build/packetbridge_receiver /tmp/pb-out
./build/packetbridge_sender 127.0.0.1 /tmp/pb-binary.bin
```

Verify:

```sh
cmp /tmp/pb-binary.bin /tmp/pb-out/received_pb-binary.bin
shasum -a 256 /tmp/pb-binary.bin /tmp/pb-out/received_pb-binary.bin
```

## Checkpoint Continuation Test

Create a partial output and checkpoint manually:

```sh
mkdir -p /tmp/pb-continue
head -c 262144 /dev/urandom > /tmp/pb-large.bin
dd if=/tmp/pb-large.bin of=/tmp/pb-continue/received_pb-large.bin bs=65536 count=1 conv=notrunc
dd if=/tmp/pb-large.bin of=/tmp/pb-continue/received_pb-large.bin bs=65536 skip=2 seek=2 count=1 conv=notrunc
cat > /tmp/pb-continue/received_pb-large.bin.pbcheckpoint <<'EOF'
PACKETBRIDGE_CHECKPOINT_V1
filename=pb-large.bin
file_size=262144
chunk_size=65536
chunk_count=4
completed=0,2
EOF
```

Run receiver and sender:

```sh
./build/packetbridge_receiver /tmp/pb-continue
./build/packetbridge_sender 127.0.0.1 /tmp/pb-large.bin
```

Expected result: the sender reports 2 chunks already present and sends 2 chunks. Verify:

```sh
cmp /tmp/pb-large.bin /tmp/pb-continue/received_pb-large.bin
test ! -f /tmp/pb-continue/received_pb-large.bin.pbcheckpoint
```

## SHA-256 Verification

Receiver logs should include:

```text
SHA-256 integrity verified
```

The digest from `shasum -a 256` should match for original and received files.
