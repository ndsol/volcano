Files in this directory were generated by running:

```
$ cd ../openssl
$ ./Configure -D__ANDROID_API__=24 \
    disable-ssl3 threads zlib no-asm \
    disable-dynamic-engine android-arm64
$ make include/openssl/opensslconf.h crypto/buildinf.h crypto/include/internal/{bn,dso}_conf.h
$ mv crypto/buildinf.h ../openssl-macos/
$ for a in crypto/include/internal/{bn,dso}_conf.h; do mv "$a" ../openssl-macos/internal/; done
$ mv include/openssl/opensslconf.h ../openssl-macos/include/openssl/
```