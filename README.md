# REmote WOrk FileSystem

[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/dsiroky/rewofs/blob/master/LICENSE.txt)
[![Build Status](https://travis-ci.org/dsiroky/rewofs.svg?branch=master)](https://travis-ci.org/dsiroky/rewofs)

- Working on a code on a remote server?
- Slow network with high latencies?
- sshfs/nfs/samba too slow?

## Features:
- Heavy client-side caching.
    - The whole served tree is preloaded - fast browsing.
    - Read files are kept in the memory.
- Remote invalidations.
    - Files/directories can be modified on the server side. Changes are
      propagated to the client.
- Auto reconnect.
