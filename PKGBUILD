# $Id: PKGBUILD 80520 2012-11-23 18:21:28Z kkeen $
# Maintainer: Kyle Keen <keenerd@gmail.com>
pkgname=jshon
pkgver=20121122
pkgrel=1
pkgdesc="A json parser for the shell."
arch=('i686' 'x86_64')
url="http://kmkeen.com/jshon/"
license=('MIT')
depends=('jansson')
source=(http://kmkeen.com/$pkgname/$pkgname-$pkgver.tar.gz)
md5sums=('b66f6b23b510fc2cb571dcb69121b24c')

build() {
  cd "$srcdir/$pkgname-$pkgver"
  make
}

package() {
  cd "$srcdir/$pkgname-$pkgver"
  install -Dm755 $pkgname "$pkgdir/usr/bin/$pkgname"
  install -Dm644 $pkgname.1 "$pkgdir/usr/share/man/man1/$pkgname.1"
}
