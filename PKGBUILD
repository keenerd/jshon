# Maintainer: Kyle Keen <keenerd@gmail.com>
pkgname=jshon
pkgver=20110306
pkgrel=1
pkgdesc="A json parser for the shell."
arch=('i686' 'x86_64')
url="http://kmkeen.com/jshon/"
license=('MIT')
depends=('jansson')
makedepends=()
optdepends=()
source=(http://kmkeen.com/$pkgname/$pkgname-$pkgver.tar.gz)
md5sums=('a047a6281147956a9422a25fdd97adf3')

build() {
  cd "$srcdir/$pkgname"
  make
}

package() {
  cd "$srcdir/$pkgname"
  install -Dm755 $pkgname "$pkgdir/usr/bin/$pkgname"
  install -Dm644 $pkgname.1 "$pkgdir/usr/share/man/man1/$pkgname.1"
}
