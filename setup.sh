#!/bin/bash
cd $(dirname $0)
make -C ./build-$(uname) clean all
./bake setup
