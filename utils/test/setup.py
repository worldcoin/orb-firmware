# Setup file

from setuptools import setup, find_packages

setup(name='orb.utils.test',
      version='0.0.1',
      description='Test utilities for Orb',
      author='Worldcoin',
      author_email='cyril.fougeray@toolsforhumanity.com',
      packages=find_packages(),
      install_requires=[
          'fabric2==3.0.0',
          'ppk2-api==0.9.1',
          'pyftdi==0.54.0'
      ],
      entry_points={
          'console_scripts': [
              'myapp=myapp.main:main'
          ]
      }
      )
