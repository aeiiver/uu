# uu

Self-hosted build system for C

### What is this?

Edit [`./uu.c`](/uu.c) as you need, but don't remove what's already in it if
you want this to work. Then bootstrap the build system:

```console
$ cc -o m ./uu.c
```

Whenever you want need to build your project, run the following command:

```console
$ ./m
```

You may run this command again without the need to rebuild `m` first because it
can do it itself.
