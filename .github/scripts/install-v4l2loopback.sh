#!/bin/bash
set -e -x

# Install v4l2loopback kernel module
sudo apt-get install -y v4l2loopback-dkms

# Create a virtual camera device for the tests
sudo modprobe v4l2loopback devices=1
