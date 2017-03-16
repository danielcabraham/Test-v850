############ User configuration ############
%define JLR_NAME		v850-fw
%define JLR_VERSION		3.2
%define JLR_PACK_RELEASE	4026

%define CARLINE         	J8A2
%define CARD            	11E013
%define JLR_RELEASE     	AB03
%define JLR_FW_VERSION  	426.000.000

%define STC_XML                 V850_ISC_FW.xml 
%define XML_PATH		/usr/local/lib/firmware
############################################
%define DEVEL_VER 		3.2
%define DEVEL_REL		4014
############################################

Name:           %{JLR_NAME}
Version: 	%{JLR_VERSION}
Release: 	%{JLR_PACK_RELEASE}
Summary:        %{CARLINE}-%{CARD}-%{JLR_RELEASE}
Group:          Development/Libraries
License:        Bosch Proprietary License
Source:         %{name}.tar.gz
ExclusiveArch: 	i586	

%define _jlropt /opt/jlr/
%define _jlrvar /var/firmware
%define _jlrbuildroot %{buildroot}%{_jlropt}
%define ISC_DNL_FILE    v850_%{CARLINE}-%{CARD}-%{JLR_RELEASE}_V.%{JLR_FW_VERSION}.dnl

%description
This package provides libvirgindownload shared library which provides API's to flash V850

Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%package -n libvirgindownload-devel
Summary: 	libvirgindownload shared library to flash V850
Group: 		Development/Libraries
Version: 	%{DEVEL_VER}
Release: 	%{DEVEL_REL}

%description -n libvirgindownload-devel
This package provides interface file for accessing API's of libvirgindownload shared library which provides API's to flash V850

%prep
%setup -q -n %{name}

%build
make sharedlib

%install
rm -rf %{buildroot}
mkdir -p %{_jlrbuildroot}/lib/v850
mkdir -p %{_jlrbuildroot}/etc/v850
mkdir -p %{buildroot}/%{_jlrvar}
mkdir -p %{buildroot}/%{XML_PATH}
mkdir -p %{buildroot}/usr/include

install -m 666 %{ISC_DNL_FILE} 			%{buildroot}/%{_jlrvar}
install -m 777 %{STC_XML}  			%{buildroot}/%{XML_PATH}
install -m 644 v850_dnl_if.h 			%{buildroot}/usr/include
ln -sf %{_jlrvar}/%{ISC_DNL_FILE} 		%{buildroot}%{_jlrvar}/master.dnl

%make_install DESTDIR=%{_jlrbuildroot}

%post 
/sbin/ldconfig %{_jlropt}/lib/v850


%postun 
/sbin/ldconfig %{_jlropt}/lib/

%files
%{_jlropt}/lib/v850/libvirgindownload*
%{_jlropt}/etc/v850/flash.cfg
%{_jlrvar}/%{ISC_DNL_FILE}
%{_jlrvar}/master.dnl
%{XML_PATH}/%{STC_XML}

%files -n libvirgindownload-devel
/usr/include/v850_dnl_if.h


%changelog
* Tue Mar 09 2015 Vigneshwaran K
 /debug operation is removed in cfg file to reduce the log file(/var/log/v850_logfile.txt) size

* Tue Feb 24 2015 Vigneshwaran K
- devel rpm is added with any dependency

* Thu Apr 03 2014 Kumar K(RBEI/ECA1)
- updated the error timeout and serial port locking

* Tue Mar 11 2014 ShashiKiran H S(RBEI/ECA1)
- included sub-package flashwriter

* Sat Nov 23 2013 ShashiKiran H S(RBEI/ECA1)
- updated the installation path to /opt/jlr

* Tue Sep 10 2013 ShashiKiran H S(RBEI/ECA1)
- VERSION 1.0-4
- Merged the test-manager feature & tested DNL generated with new toolchain

* Thu May 23 2013 ShashiKiran H S(RBEI/ECG2)
- Added sub-packages libvirgindownload-devel and test application

* Wed Mar 20 2013 Satyanarayana Venkatesh(RBEI/ECG2)
- Initial VERSION

