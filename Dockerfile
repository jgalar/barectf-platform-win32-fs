FROM dockcross/windows-static-x64

LABEL maintainer="jeremie.galarneau@gmail.com"

# tzdata, installed by asciidoc, will ask for a timezone during install
#ENV DEBIAN_FRONTEND noninteractive
ARG BARECTF_IMAGE_TAG
ENV DEFAULT_DOCKCROSS_IMAGE=$BARECTF_IMAGE_TAG

# Upgrade base image to Debian Buster since barectf 3.0 requires python >= 3.6
# and the dockcross image is based on Debian Stretch which provides python 3.5
RUN apt-get update
RUN apt-get --yes upgrade
RUN apt-get --yes dist-upgrade
RUN sed -i 's/stretch/buster/g' /etc/apt/sources.list
RUN apt-get update
RUN apt-get --yes upgrade
RUN apt-get --yes dist-upgrade
RUN apt install --yes python3-pip
RUN pip3 install barectf
RUN apt-get --yes install bear
