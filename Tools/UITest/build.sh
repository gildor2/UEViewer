#!/bin/bash

# VC2013 has problems with using lambdas in BIND_LAMBDA, so use VS2015
vc_ver=2015
project="uitest"
root="../.."
render=0
source $root/build.sh
