### To compile and install the library

```
make && make install
```

The header, httpcpp.h, and the library, ./lib/libhttpcpp.a, will be placed in
/usr/local/include and /usr/local/lib, respectively.

### To produce the example

```
make example
```

The binary will be available in directory ./bin. To see how to program HTTP 
clients/servers using the APIs provided by the library, look at the code in 
example.cpp.

### Disclaimer

I am by no means a C++ expert, but I try my best to write a simple and 
easy-to-understand code for the greatest benefit of mankind (sounds a little 
too big now!). What I am trying to say is if you find bugs (or believed to be 
ones) or have suggestion, please feel free to communicate to me by creating 
issues in GitHub. Of course, pull requests are warmly welcome.
