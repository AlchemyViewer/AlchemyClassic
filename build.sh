#!/bin/sh

# This is a the master build script - it is intended to be run by parabuild
# It is called by a wrapper script in the shared repository which sets up
# the environment from the various BuildParams files and does all the build 
# result post-processing.
#
# PLEASE NOTE:
#
# * This script is interpreted on three platforms, including windows and cygwin
#   Cygwin can be tricky....
# * The special style in which python is invoked is intentional to permit
#   use of a native python install on windows - which requires paths in DOS form
# * This script relies heavily on parameters defined in BuildParams
# * The basic convention is that the build name can be mapped onto a mercurial URL,
#   which is also used as the "branch" name.

check_for()
{
    if [ -e "$2" ]; then found_dict='FOUND'; else found_dict='MISSING'; fi
    echo "$1 ${found_dict} '$2' " 1>&2
}

build_dir_Darwin()
{
  echo build-darwin-i386
}

build_dir_Linux()
{
  echo build-linux-i686
}

build_dir_CYGWIN()
{
  echo build-vc100
}

installer_Darwin()
{
  ls -1tr "$(build_dir_Darwin ${last_built_variant:-Release})/newview/"*"$additional_package_name"*.dmg 2>/dev/null | sed 1q
}

installer_Linux()
{
  ls -1tr "$(build_dir_Linux ${last_built_variant:-Release})/newview/"*"$additional_package_name"*.tar.bz2 2>/dev/null | grep -v symbols | sed 1q
}

installer_CYGWIN()
{
  v=${last_built_variant:-Release}
  d=$(build_dir_CYGWIN $v)
  if [ -r "$d/newview/$additional_package_name$v/touched.bat" ]
  then
    p=$(sed 's:.*=::' "$d/newview/$additional_package_name$v/touched.bat")
    echo "$d/newview/$additional_package_name$v/$p"
  fi
}

pre_build()
{
  local variant="$1"
  begin_section "Pre$variant"
    [ -n "$master_message_template_checkout" ] \
    && [ -r "$master_message_template_checkout/message_template.msg" ] \
    && template_verifier_master_url="-DTEMPLATE_VERIFIER_MASTER_URL=file://$master_message_template_checkout/message_template.msg"

    check_for "Confirm dictionaries are installed before 'autobuild configure'" ${build_dir}/packages/dictionaries

    "$AUTOBUILD" configure -c $variant -- \
     -DPACKAGE:BOOL=ON \
     -DRELEASE_CRASH_REPORTING:BOOL=ON \
     -DVIEWER_CHANNEL:STRING="\"$viewer_channel\"" \
     -DGRID:STRING="\"$viewer_grid\"" \
     -DLL_TESTS:BOOL="$run_tests" \
     -DTEMPLATE_VERIFIER_OPTIONS:STRING="$template_verifier_options" $template_verifier_master_url

 end_section "Pre$variant"
}

package_llphysicsextensions_tpv()
{
  begin_section "PhysicsExtensions_TPV"
  tpv_status=0
  if [ "$variant" = "Release" ]
  then 
      llpetpvcfg=$build_dir/packages/llphysicsextensions/autobuild-tpv.xml
      "$AUTOBUILD" build --verbose --config-file $llpetpvcfg -c Tpv
      
      # capture the package file name for use in upload later...
      PKGTMP=`mktemp -t pgktpv.XXXXXX`
      trap "rm $PKGTMP* 2>/dev/null" 0
      "$AUTOBUILD" package --verbose --config-file $llpetpvcfg > $PKGTMP
      tpv_status=$?
      sed -n -e 's/^wrote *//p' $PKGTMP > $build_dir/llphysicsextensions_package
  else
      echo "Do not provide llphysicsextensions_tpv for $variant"
      llphysicsextensions_package=""
  fi
  end_section "PhysicsExtensions_TPV"
  return $tpv_status
}

build()
{
  local variant="$1"
  if $build_viewer
  then
    begin_section "Viewer$variant"

    "$AUTOBUILD" build --no-configure -c $variant
    build_ok=$?
    end_section "Viewer$variant"

    # Run build extensions
    if [ $build_ok -eq 0 -a -d ${build_dir}/packages/build-extensions ]; then
        for extension in ${build_dir}/packages/build-extensions/*.sh; do
            . $extension
            if [ $build_ok -ne 0 ]; then
                break
            fi
        done
    fi

    # *TODO: Make this a build extension.
    package_llphysicsextensions_tpv
    tpvlib_build_ok=$?
    if [ $build_ok -eq 0 -a $tpvlib_build_ok -eq 0 ]
    then
      echo true >"$build_dir"/build_ok
    else
      echo false >"$build_dir"/build_ok
    fi
  fi
}

# This is called from the branch independent script upon completion of all platform builds.
build_docs()
{
  begin_section Docs
  # Stub code to generate docs
  echo Hello world  > documentation.txt
  upload_item docs documentation.txt text/plain
  end_section Docs
}


# Check to see if we were invoked from the wrapper, if not, re-exec ourselves from there
if [ "x$arch" = x ]
then
  top=`hg root`
  if [ -x "$top/../buildscripts/hg/bin/build.sh" ]
  then
    exec "$top/../buildscripts/hg/bin/build.sh" "$top"
  else
    cat <<EOF
This script, if called in a development environment, requires that the branch
independent build script repository be checked out next to this repository.
This repository is located at http://hg.lindenlab.com/parabuild/buildscripts
EOF
    exit 1
  fi
fi

# Check to see if we're skipping the platform
eval '$build_'"$arch" || pass

if [ -z "$AUTOBUILD" ]
then
  export autobuild_dir="$here/../../../autobuild/bin/"
  if [ -d "$autobuild_dir" ]
  then
    export AUTOBUILD="$autobuild_dir"autobuild
    if [ -x "$AUTOBUILD" ]
    then
      # *HACK - bash doesn't know how to pass real pathnames to native windows python
      case "$arch" in
      CYGWIN) AUTOBUILD=$(cygpath -u $AUTOBUILD.cmd) ;;
      esac
    else
      record_failure "Not executable: $AUTOBUILD"
      exit 1
    fi
  else
    record_failure "Not found: $autobuild_dir"
    exit 1
  fi
fi

# load autbuild provided shell functions and variables
eval "$("$AUTOBUILD" source_environment)"

# dump environment variables for debugging
env|sort

# Now run the build
succeeded=true
build_processes=
last_built_variant=
for variant in $variants
do
  eval '$build_'"$variant" || continue
  eval '$build_'"$arch"_"$variant" || continue

  # Only the last built arch is available for upload
  last_built_variant="$variant"

  begin_section "Do$variant"
  build_dir=`build_dir_$arch $variant`
  build_dir_stubs="$build_dir/win_setup/$variant"

  begin_section "PreClean"
  rm -rf "$build_dir"
  end_section "PreClean"

  mkdir -p "$build_dir"
  mkdir -p "$build_dir/tmp"

  if pre_build "$variant" "$build_dir" >> "$build_log" 2>&1
  then
    if $build_link_parallel
    then
      begin_section BuildParallel
      ( build "$variant" "$build_dir" > "$build_dir/build.log" 2>&1 ) &
      build_processes="$build_processes $!"
      end_section BuildParallel
    else
      begin_section "Build$variant"
      build "$variant" "$build_dir" 2>&1 | tee -a "$build_log" | sed -n 's/^ *\(##teamcity.*\)/\1/p'
      if `cat "$build_dir/build_ok"`
      then
        echo so far so good.
      else
        record_failure "Build of \"$variant\" failed."
      fi
      end_section "Build$variant"
    fi
  else
    record_failure "Build Prep for \"$variant\" failed."
  fi
  end_section "Do$variant"
done

# If we are building variants in parallel, wait, then collect results.
# This requires that the build dirs are variant specific
if $build_link_parallel && [ x"$build_processes" != x ]
then
  begin_section WaitParallel
  wait $build_processes
  for variant in $variants
  do
    eval '$build_'"$variant" || continue
    eval '$build_'"$arch"_"$variant" || continue

    begin_section "Build$variant"
    build_dir=`build_dir_$arch $variant`
    build_dir_stubs="$build_dir/win_setup/$variant"
    tee -a $build_log < "$build_dir/build.log" | sed -n 's/^ *\(##teamcity.*\)/\1/p'
    if `cat "$build_dir/build_ok"`
    then
      echo so far so good.
    else
      record_failure "Parallel build of \"$variant\" failed."
    fi
    end_section "Build$variant"
  done
  end_section WaitParallel
fi

# build debian package
if [ "$arch" == "Linux" ]
then
  if $succeeded
  then
    if $build_viewer_deb && [ "$last_built_variant" == "Release" ]
    then
      begin_section "Build Viewer Debian Package"
      local have_private_repo=false
      # mangle the changelog
      dch --force-bad-version \
          --distribution unstable \
          --newversion "${VIEWER_VERSION}" \
          "Automated build #$build_id, repository $branch revision $revision." \
          >> "$build_log" 2>&1

      # build the debian package
      $pkg_default_debuild_command  >>"$build_log" 2>&1 || record_failure "\"$pkg_default_debuild_command\" failed."

      # Unmangle the changelog file
      hg revert debian/changelog

      end_section "Build Viewer Debian Package"

      # Run debian extensions
      if [ -d ${build_dir}/packages/debian-extensions ]; then
          for extension in ${build_dir}/packages/debian-extensions/*.sh; do
              . $extension
          done
      fi
      # Move any .deb results.
      mkdir -p ../packages_public
      mkdir -p ../packages_private
      mv ${build_dir}/packages/*.deb ../packages_public 2>/dev/null || true
      mv ${build_dir}/packages/packages_private/*.deb ../packages_private 2>/dev/null || true

      # upload debian package and create repository
      begin_section "Upload Debian Repository"
      for deb_file in `/bin/ls ../packages_public/*.deb ../*.deb 2>/dev/null`; do
        upload_item debian $deb_file binary/octet-stream
      done
      for deb_file in `/bin/ls ../packages_private/*.deb 2>/dev/null`; do
        upload_item debian_private $deb_file binary/octet-stream
        have_private_repo=true
      done

      create_deb_repo

      # Rename the local debian_repo* directories so that the master buildscript
      # doesn't make a remote repo again.
      for debian_repo_type in debian_repo debian_repo_private; do
        if [ -d "$build_log_dir/$debian_repo_type" ]; then
          mv $build_log_dir/$debian_repo_type $build_log_dir/${debian_repo_type}_pushed
        fi
      done

      if [ $have_private_repo = true ]; then
        eval "$python_command \"$redirect\" '\${private_S3PROXY_URL}${S3PREFIX}repo/$repo/rev/$revision/index.html'"\
            >"$build_log_dir/private.html" || fatal generating redirect
        upload_item global_redirect "$build_log_dir/private.html" text/html
        
      fi

      end_section "Upload Debian Repository"
      
    else
      echo skipping debian build
    fi
  else
    echo skipping debian build due to failed build.
  fi
fi


# check status and upload results to S3
if $succeeded
then
  if $build_viewer
  then
    begin_section Upload Installer
    # Upload installer
    package=$(installer_$arch)
    if [ x"$package" = x ] || test -d "$package"
    then
      # Coverity doesn't package, so it's ok, anything else is fail
      succeeded=$build_coverity
    else
      # Upload base package.
      upload_item installer "$package" binary/octet-stream
      upload_item quicklink "$package" binary/octet-stream
      [ -f $build_dir/summary.json ] && upload_item installer $build_dir/summary.json text/plain

      # Upload additional packages.
      for package_id in $additional_packages
      do
        case $arch in
        CYGWIN) export additional_package_name="$package_id/" ;;
        *) export additional_package_name=$package_id ;;
        esac
        package=$(installer_$arch)
        if [ x"$package" != x ]
        then
          upload_item installer "$package" binary/octet-stream
        else
          record_failure "Failed to upload $package_id package ($package::$additional_package_name)."
        fi
      done
      export additional_package_name=""

      case "$last_built_variant" in
      Release)
        # Upload crash reporter files
        for symbolfile in $symbolfiles
        do
          upload_item symbolfile "$build_dir/$symbolfile" binary/octet-stream
        done

        # Upload the llphysicsextensions_tpv package, if one was produced
        # *TODO: Make this an upload-extension
        if [ -r "$build_dir/llphysicsextensions_package" ]
        then
            llphysicsextensions_package=$(cat $build_dir/llphysicsextensions_package)
            upload_item private_artifact "$llphysicsextensions_package" binary/octet-stream
        else
            echo "No llphysicsextensions_package"
        fi
        ;;
      *)
        echo "Skipping mapfile for $last_built_variant"
        ;;
      esac

      # Run upload extensions
      if [ -d ${build_dir}/packages/upload-extensions ]; then
          for extension in ${build_dir}/packages/upload-extensions/*.sh; do
              . $extension
          done
      fi

      # Upload stub installers
      upload_stub_installers "$build_dir_stubs"
    fi
    end_section Upload Installer
  else
    echo skipping upload of installer
  fi
else
  echo skipping upload of installer due to failed build.
fi

# The branch independent build.sh script invoking this script will finish processing
$succeeded || exit 1
