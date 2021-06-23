FROM debian:stable

ENV REFRESHED_AT 2021-06-23
ENV DOCKER_RUN 1

# Set up a tools dev directory
RUN mkdir /home/dev

WORKDIR /home/project

RUN apt-get update && apt-get install -y python3-pip git bc wget
RUN wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh

RUN mkdir /root/.conda \
    && bash Miniconda3-latest-*.sh -b \
    && rm -f Miniconda3-latest-*.sh

ENV PATH $PATH:/root/miniconda3/bin

# Create the environment
COPY environment.yml /home/dev/environment.yml
RUN conda env create -f /home/dev/environment.yml

## use the new environment without activating with Conda
ENV PATH $PATH:/root/miniconda3/envs/worldcoin/bin/
