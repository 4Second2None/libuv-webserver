
all: ./build ./webclient ./webserver

./deps/http-parser:
	git clone --depth 1 git://github.com/joyent/http-parser.git ./deps/http-parser

./deps/libuv:
	git clone --depth 1 git://github.com/joyent/libuv.git ./deps/libuv

./deps/gyp:
	git clone --depth 1 https://chromium.googlesource.com/external/gyp.git ./deps/gyp

./build: ./deps/gyp ./deps/libuv ./deps/http-parser
	deps/gyp/gyp --depth=. -Goutput_dir=./out -Icommon.gypi --generator-output=./build -Dlibrary=static_library -f make

./webclient:
	make -C ./build/ webclient
	cp ./build/out/Release/webclient ./webclient

./webserver:
	make -C ./build/ webserver
	cp ./build/out/Release/webserver ./webserver

distclean:
	rm -rf ./build

test:
	./build/out/Release/webserver & ./build/out/Release/webclient

clean:
	rm -rf ./build/out/Release/obj.target/webserver/
	rm -f ./build/out/Release/webserver
	rm -f ./webserver
	rm -rf ./build/out/Release/obj.target/webclient/
	rm -f ./build/out/Release/webclient
	rm -f ./webclient

.PHONY: test