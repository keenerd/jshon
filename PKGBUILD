# Contributor: Kyle Keen <keenerd@gmail.com>
pkgname=jshon
pkgver=20110215
pkgrel=1
pkgdesc="A json parser for the shell."
arch=('i686' 'x86_64')
url="http://kmkeen.com/jshon/"
license=('MIT')
depends=('jansson')
makedepends=()
optdepends=()
source=(http://kmkeen.com/$pkgname/$pkgname-$pkgver.tar.gz)
md5sums=('d3bc9f0171193b84f75967c3b3744bc6')

build() {
  cd "$srcdir/$pkgname"
  make
  gzip "$pkgname.1"
  install -D -m 0755 $pkgname "$pkgdir/usr/bin/$pkgname"
  install -D -m 0644 "$pkgname.1.gz" "$pkgdir/usr/share/man/man1/$pkgname.1.gz"
}
