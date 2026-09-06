#!/usr/bin/env bash

case "$BUILD_TARGET" in
"vita")
	echo "PlayStation Vita is outside Vespasian's hardware requirements." >&2
	exit 1
	;;
"switch")
	echo "The original Switch target is retired. Switch 2 needs its own validated toolchain; retained Switch adapters are reference code." >&2
	exit 1
	;;
"android")
	# Decrypt the key files
	if [ "$FILE_ENCRYPTION_KEY" ]
	then
        openssl aes-256-cbc -K $FILE_ENCRYPTION_KEY -iv $FILE_ENCRYPTION_IV -in android/augustus.keystore.enc -out android/augustus.keystore -d;
        # openssl aes-256-cbc -K $FILE_ENCRYPTION_KEY -iv $FILE_ENCRYPTION_IV -in android/play-publisher.json.enc -out android/play-publisher.json -d;
	fi
	;;
"emscripten")
	# Get EMSDK
	git clone https://github.com/emscripten-core/emsdk.git
	cd emsdk
	./emsdk install latest
	./emsdk activate latest
	;;
esac
