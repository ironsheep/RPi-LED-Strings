#!/bin/bash

(set -x;./autogen.sh)
(set -x;./configure --prefix=/some_directory)
(set -x;make)
# (set -x;make install)
