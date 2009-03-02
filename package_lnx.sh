#!/bin/bash
rm -f umodel_linux.tar.gz
tar -cf - umodel readme.txt | gzip -f9 > umodel_linux.tar.gz
