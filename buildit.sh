#!/bin/bash

(set -x;./autogen.sh)
(set -x;./configure --prefix=/usr/local)
(set -x;make)
# (set -x;make install)
