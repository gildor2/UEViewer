#!/bin/bash
tar -cf - umodel readme.txt | gzip -f9 > umodel_linux.tar.gz
