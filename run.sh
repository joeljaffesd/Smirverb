#!/bin/bash

set -e 

cd build

cmake ..

cmake --build . --parallel

open SMIRVERB_artefacts/Standalone/SMIRVERB.app