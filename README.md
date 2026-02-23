IcoWM
=====

**IcoWM** is a minimalist stacking window manager for the X Window
System that was meticulously crafted without the inclusion of desktop
icons for inactive windows, placing a significant emphasis on the
utilization of iconification (iconization) in a manner reminiscent of
traditional TWM aesthetics, but with a modern touch.

Basic features are:

  - **Support for multiple monitors.**
    Integration and management of multiple display devices to extend the
    desktop environment across various screens.

  - **Configurable keyboard and mouse controls.**
    Full flexibility to customize input methods tailoring keyboard
    shortcuts and mouse actions.

  - **Virtual Desktops.**
    Organization of open applications into discrete workspaces
    minimizing visual clutter.

  - **Extended Window Manager Hints (EWMH) compliance.**
    As it provides needed compatibility.

  - **Dynamic configuration management.**
    Configuration files are read upon initialization and can
    subsequently be reloaded in response to a `SIGHUP` signal.

  - **Iconifying (classical).**
    Instead of classical minimization on the taskbar, the window is
    iconified on the desktop in TWM-style.

The primary goal of this endeavor is to create a window manager that can
be entirely navigated  through keyboard commands, whilst still
accommodating optional mouse interaction.  This dual capability fosters
an environment conducive to efficiency, especially for those users who
prefer the elegance of keyboard-driven workflows.  The icon
functionality has been retained despite its diminished prevalence in
contemporary interfaces, for this feature harkens back to an era when
applications were elegantly transformed into icons, which is a stylistic
choice that has largely been overshadowed by contemporary minimization
practices into a crowded taskbar.  Drawing inspiration from classical
window managers such as TWM, evilwm, and Openbox, this window manager
aspires to blend a lightweight design *ethos* with usability.

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
