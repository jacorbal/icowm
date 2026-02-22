IcoWM
=====

Minimalist stacking window manager for the X Window System.  It was
meticulously crafted without the inclusion of desktop icons for inactive
windows, placing a significant emphasis on the utilization of
iconification (iconization) in a manner reminiscent of traditional TWM
aesthetics.

Basic features are:

  - **Support for multiple monitors.**
    This functionality allows for seamless integration and management of
    multiple display devices, enabling users to extend their desktop
    environment across various screens.

  - **Configurable keyboard and mouse controls.**
    Users are granted the flexibility to customize their input methods,
    tailoring keyboard shortcuts and mouse actions according to their
    individual preferences and workflows.

  - **Virtual Desktops.**
    This feature facilitates the organization of open applications into
    discrete workspaces, allowing users to maintain a tidy and efficient
    workflow by categorizing tasks in a manner that minimizes visual
    clutter.

  - **EWMH compliance.**
    The implementation of the Extended Window Manager Hints (EWMH)
    ensures compatibility with various desktop environments and
    applications, promoting a cohesive and standardized user experience.

  - **Dynamic configuration management.**
    The configuration file is read upon initialization and can
    subsequently be reloaded in response to a `SIGHUP` signal, providing
    users with the convenience of adjusting settings without
    necessitating a complete restart of the window manager.

  - **Iconifying (classical).**
    Instead of classical minimization on the taskbar, the window is
    iconified on the desktop.

The primary objective of this endeavor is to establish a window manager
that can be navigated entirely through keyboard commands, whilst still
accommodating optional mouse interaction.  This dual capability fosters
an environment conducive to efficiency, particularly for those users who
prefer the elegance of keyboard-driven workflows.  Furthermore, the icon
functionality has been retained, despite its diminished prevalence in
contemporary interfaces.  This feature harkens back to an era when
applications were elegantly transformed into icons, a stylistic choice
that has largely been overshadowed by modern minimization practices.

Drawing inspiration from classical window managers such as TWM, evilwm,
and Openbox, this window manager aspires to harmoniously blend
a lightweight design *ethos* with exceptional usability.  By
prioritizing keyboard navigation and reintroducing the nostalgic element
of iconification, it seeks to cater to discerning users who value both
minimalism and functionality within their desktop environments.

Ethical statement
-----------------

  - No part of IcoWM was written using AI of any type, and it never
    will.
  - No AI-generated patches or other sorts of contributions to IcoWM
    will be accepted.
  - IcoWM is licensed under the ISC License; however, the code may not
    be used to train an AI model, including an LLM model, unless all
    outputs of said model are released under the following licenses: MIT
    License; ISC License; BSD 4-Clause License; BSD 3-Clause License,
    a.k.a. "Revised BSD License"; BSD 2-Clause License,
    a.k.a. "Simplified BSD License"; or GNU General Public License,
    version 2; alongst with any license that is compatible with the ones
    listed above.
  - These policies are in place to maintain the integrity and quality of
    IcoWM, ensuring that the project remains free from influences that
    may compromise its *human* vision.

License
-------

This software is licensed under the 'ISC License'.
Read the [`LICENSE`](LICENSE) file on this repository, or gather more
information on [ISC Open Source Software
Licenses](https://www.isc.org/licenses/).

Copyright (c) 2026, J. A. Corbal.

Contact information
-------------------

  - GitHub repository: <https://github.com/jacorbal/icowm/>
  - Web page: <https://jacorbal.org/icowm/>
  - E-mail: <jacorbal@protonmail.com>
