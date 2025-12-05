#!/bin/bash

# If the service is masked, unmask it first
sudo systemctl unmask mynd-rpi-link

# Stop and disable the service
sudo systemctl stop mynd-rpi-link
sudo systemctl disable mynd-rpi-link

# Remove the files
sudo rm /etc/systemd/system/mynd-rpi-link.service
sudo rm /usr/local/bin/mynd_rpi_link.py
sudo rm /etc/mynd_rpi_link.conf
sudo rm /etc/sudoers.d/mynd-rpi-link

# Reload systemd
sudo systemctl daemon-reload