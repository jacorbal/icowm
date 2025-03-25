IcoWM
=====

Minimalist stacking window manager for the X Window System.  It was
meticulously crafted without the inclusion of desktop icons, placing
a significant emphasis on the utilization of iconization in a manner
reminiscent of traditional TWM aesthetics.

Basic features are:

  - Support for Multiple Monitors.
    This functionality allows for seamless integration and management of
    multiple display devices, enabling users to extend their desktop
    environment across various screens.

  - Configurable Keyboard and Mouse Controls.
    Users are granted the flexibility to customize their input methods,
    tailoring keyboard shortcuts and mouse actions according to their
    individual preferences and workflows.

  - EWMH Compliance.
    The implementation of the Extended Window Manager Hints (EWMH)
    ensures compatibility with various desktop environments and
    applications, promoting a cohesive and standardized user experience.

  - Virtual Desktops.
    This feature facilitates the organization of open applications into
    discrete workspaces, allowing users to maintain a tidy and efficient
    workflow by categorizing tasks in a manner that minimizes visual
    clutter.

  - Dynamic Configuration Management.
    The configuration file is read upon initialization and can
    subsequently be reloaded in response to a SIGHUP signal, providing
    users with the convenience of adjusting settings without
    necessitating a complete restart of the window manager.

The primary objective of this endeavor is to establish a window manager
that can be navigated entirely through keyboard commands, while still
accommodating optional mouse interaction.  This dual capability fosters
an environment conducive to efficiency, particularly for those users who
prefer the elegance of keyboard-driven workflows.  Furthermore, the icon
functionality has been retained, despite its diminished prevalence in
contemporary interfaces.  This feature harkens back to an era when
applications were elegantly transformed into icons, a stylistic choice
that has largely been overshadowed by modern minimization practices.

Drawing inspiration from classical window managers such as TWM, evilwm,
and Openbox, this window manager aspires to harmoniously blend
a lightweight design ethos with exceptional usability.  By prioritizing
keyboard navigation and reintroducing the nostalgic element of
iconization, it seeks to cater to discerning users who value both
minimalism and functionality within their desktop environments.

License
=======

This software is licensed using ISC Open Source Software License.  Read
the `COPYING` file or gather more information on [ISC Licenses
website](https://www.isc.org/licenses/).

Copyright (c) 2025, J. A. Corbal.
