# YMODEM on Unix machine

## Minicom

On Linux and MacOS, we need to use minicom instead of TeraTerm to transfer the image.

First, make sure to compile the image for Minicom by adding the macro definition `MINICOM_YMODEM` when compiling.

### MacOS

Behind the scene, Minicom uses `rb`/`rz`/`rs` but MacOS doesn't come with those so you need to install `lrzs`:
```shell
brew install lrzs
```

Then, in Minicom, type `Escape` `O` > `File transfer protocols` and set `lsb` and `lrb` instead of `sb` and `rb`:

```shell
┌──────────────────────────────────────────────────────────────────────────────┐
│     Name             Program                 Name U/D FullScr IO-Red. Multi  │
│ A  zmodem     sz -vv -b                       Y    U    N       Y       Y    │
│ B  ymodem     lsb -y -vv                      Y    U    N       Y       Y    │
│ C  xmodem     sx -vv                          Y    U    N       Y       N    │
│ D  zmodem     rz -vv -b -E                    N    D    N       Y       Y    │
│ E  ymodem     lrb -y -vv                      N    D    N       Y       Y    │
│ F  xmodem     rx -vv                          Y    D    N       Y       N    │
│ G  kermit     kermit -i -l %l -b %b -s        Y    U    Y       N       N    │
│ H  kermit     kermit -i -l %l -b %b -r        N    D    Y       N       N    │
│ I  ascii      ascii-xfr -dsv                  Y    U    N       Y       N    │
│ J    -                                                                       │
│ K    -                                                                       │
│ L    -                                                                       │
│ M  Zmodem download string activates... D                                     │
│ N  Use filename selection window...... Yes                                   │
│ O  Prompt for download directory...... No                                    │
│                                                                              │
│   Change which setting? (SPACE to delete)                                    │
└──────────────────────────────────────────────────────────────────────────────┘
```

