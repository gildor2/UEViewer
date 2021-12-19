This directory contains SDK for oodle. It could be obtained from the Unreal Engine 4.27 or newer source code repository.

The directory structure should appear like this:

```
├── include               # header files from oodle SDK
├── lib                   # library files from oodle SDK
│   ├── Linux
│   ├── Mac
│   ├── Win32
│   └── Win64
└── README.md             # this file
```

You may use symlink to provide the following directories here, or simply copy the required files here. In a case the SDK is
not available for the platform you're working with, the oodle support will be automatically disabled.
