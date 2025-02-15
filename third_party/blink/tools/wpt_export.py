#!/usr/bin/env vpython
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pushes changes to web-platform-tests inside Chromium to the upstream repo."""

import os
import sys

# Without abspath(), PathFinder can't find chromium_base correctly.
sys.path.append(os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..',
                 'WebKit', 'Tools', 'Scripts')))
from webkitpy.common import exit_codes
from webkitpy.common.host import Host

from webkitpy.w3c.test_exporter import TestExporter


def main():
    host = Host()
    exporter = TestExporter(host)
    try:
        success = exporter.main()
        host.exit(0 if success else 1)
    except KeyboardInterrupt:
        host.print_('Interrupted, exiting')
        host.exit(exit_codes.INTERRUPTED_EXIT_STATUS)


if __name__ == '__main__':
    main()
