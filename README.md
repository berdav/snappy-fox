# Snappy-fox
Snappy-fox is a morgue cache decompressor for Firefox wich does not have
dependencies.

## Why?
Online applications such as whatsapp web (web.whatsapp.com) saves cache
files (e.g. images) in a compressed way.

You can recognize these files searching for the pattern sNaPpY:
```bash
grep 'sNaPpY' ~/.mozilla/firefox/*/storage/default/*/cache/morgue/*/*
```

This application can help the retrieval of such cache files.

## Setup
Just compile this application, you will need a C compiler
(gcc or clang) and make
```bash
sudo apt install make gcc
```

Then just compile the application
```bash
make
```

You can add debug prints with
```bash
make CFLAGS=DEBUG
```

## How?

The usage of the application is pretty simple:
```bash
$ snappy-fox <input-file> <output-file>
```
The input or the output files can be `-` to use, respectively stdin and
stdout.

For instance you can do:
```bash
mkdir /tmp/extracted-cache-whatsapp
for f in
  find ~/.mozilla/firefox/**/storage/default/https+++web.whatsapp.com/cache/ -name '*.final';
  do
    snappy-fox "$f" "/tmp/extracted-cache-whatsapp/$(basename $f)"
done
```

And it will extract all your cache files in the
`/tmp/extracted-cache-whatsapp` directory.
