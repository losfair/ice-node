#!/bin/bash

wget -O libice_core.so https://github.com/losfair/IceCore/releases/download/v0.1.1/libice_core.so
node-gyp rebuild
