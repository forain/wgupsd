all: wgupsd

wgupsd: wgupsd.c
        gcc -o wgupsd wgupsd.c

clean:
        rm -rf wgupsd
