![PyTorch Logo](https://github.com/pytorch/pytorch/blob/master/docs/source/_static/img/pytorch-logo-dark.png)

--------------------------------------------------------------------------------

To install/uninstall Pytorch On AIX

Install Dependencies:

- pip install setuptools six future PyYAML numpy protobuf

Steps to build and install pytorch from source:

- bash

- git clone --recursive https://github.com/kavanabhat/pytorch.git -b v1.0.1

- cd pytorch

- source ./aix_setup.sh

- USE_DISTRIBUTED=OFF python setup.py install

Steps to uninstall pytorch:

- pip uninstall torch

Sample testcase:

- export LIBPATH=/opt/freeware/lib64/python3.7/site-packages/torch/lib:/opt/freeware/lib/pthread/ppc64 

- cd caffe2/python/examples

- wget https://caffe2.ai/static/datasets/shakespeare.txt

- python char_rnn.py --train_data shakespeare.txt

