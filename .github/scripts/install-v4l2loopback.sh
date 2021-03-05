#!/bin/bash
set -x

# Install v4l2loopback kernel module
sudo apt-get install -y v4l2loopback-dkms
cat /var/lib/dkms/v4l2loopback/0.10.0/build/make.log

# Create a virtual camera device for the tests
sudo modprobe v4l2loopback devices=1
