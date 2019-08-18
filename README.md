# System Menu Hook
This is a payload that should be run with [MochaLite](https://github.com/wiiu-env/MochaLite) before the System Menu.
It's exploits the Cafe OS and maps 8 MiB of usable memory from 0x30000000...0x30800000 (physical address) to 0x00800000... 0x01000000 (virtual address) where a payload will be loaded.
The loaded `hook_payload.elf` needs to be mapped to this memory area.

## Usage
Put the `root.rpx` in the `sd:/wiiu/` folder of your sd card and start MochaLite.

This will load another payload from the sd card `sd:/wiiu/hook_payload.elf` and install it as a main()-hook. An example payload is the [Wii U Plugin System Backend](https://github.com/wiiu-env/WiiUPluginLoaderBackend).

## Building
Make you to have [wut](https://github.com/devkitPro/wut/) installed and use the following command for build:
```
make install
```

## Credits
orboditilt
dimok (gx2 exploit implementation)