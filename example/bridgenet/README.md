# Simple connect example

This example creates a simple connection between two clients.
These two clients exchange connection description and information about candidates using a public mqtt server.

## Client 1
```
idf.py -B build1 -DSDKCONFIG=build1/sdkconfig -DCLIENT1=1 menuconfig build flash monitor
```

## Client 2
```
idf.py -B build2 -DSDKCONFIG=build2/sdkconfig -DCLIENT2=1 menuconfig build flash monitor
```

