Name:		jshon
Version:	20131105
Release:	0%{?dist}
Summary:	Jshon is a JSON parser designed for maximum convenience within the shell

Group:		Applications/System
License:	MIT
URL:		http://kmkeen.com/jshon
Source0:	jshon-%{version}.tar.gz
Patch0:		jshon-makefile-enable-install.patch
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:  gcc >= 3.4.6, jansson

Requires:	jansson

%description
jshon

%prep
%setup -q
%patch0 -p0


%build
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%{_bindir}
%doc %_mandir/man1/jshon.1.gz



%changelog
* Tue Oct  1 2013 Jiri Horky <horky@avast.com> 
- Initial spec file
