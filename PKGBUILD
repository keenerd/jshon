# $Id$
# Maintainer: Kyle Keen <keenerd@gmail.com>
pkgname=jshon
pkgver=20111222
pkgrel=1
pkgdesc="A json parser for the shell."
arch=('i686' 'x86_64')
url="http://kmkeen.com/jshon/"
license=('MIT')
depends=('jansson')
makedepends=()
optdepends=()
source=(http://kmkeen.com/$pkgname/$pkgname-$pkgver.tar.gz)
md5sums=('bcc8b8b6e8c09af7d053471a473ef6e8')

build() {
  cd "$srcdir/$pkgname"
  make
}

package() {
  cd "$srcdir/$pkgname"
  install -Dm755 $pkgname "$pkgdir/usr/bin/$pkgname"
  install -Dm644 $pkgname.1 "$pkgdir/usr/share/man/man1/$pkgname.1"
}
