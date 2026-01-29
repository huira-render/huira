============================================
Choosing a Frame for Aberration Calculations
============================================

TLDR: For *all* light-time delay and velocity aberration calculations, Huira uses velocities expressed in the J2000 frame, centered on the Solar System Barycenter (SSB).  If you accept the validity of this choice, you can skip the rest of this document.  If you want to understand *why* this is the (or really, "a") correct choice, read on.

Because of the finite speed of light, where something *appears* to be in an image is not necessarily where it actually is "right now".  There are two main effects that cause this:
- **The geometric light-time delay:** because light takes time to travel from the target to the observer, we see the target where it *was* when the light left it, not where it is "now".
- **The velocity aberration:** because the observer is moving relative to the incoming light, the apparent direction the light is coming from is shifted.

Notice that each of these effects depends on different things: the geometric light-time delay depends on the position and velocity of the *target* (to figure out where it was when the light left it), while the velocity aberration depends on the velocity of the *observer* (to figure out how the incoming light direction is shifted).  But this raises a question: If each effect depends on velocity, then wouldn't the effect depend on what inertial frame we choose to express those velocities in?

Obviously in reality, there is no ambiguity.  The observer doesn't need to do any math, in order to *see*.  But it can be tempting to think:  **If you are sitting at your desk looking at your computer monitor, you would not expect to see any aberration, since you can clearly consider yourself to be at rest.  But if you consider that the Earth is orbiting the Sun at about 30km/s, then in a Solar System Barycenter (SSB) frame, you *are* moving, and so you *should* see aberration.**

It is often hand-waved away, by simply saying that the aberration applies to directions defined within the frame you're computing in, or by invoking relativity, which tells us that there is no preferred inertial frame.  While this is true, it doesn't (on it's surface) seem to address the problem outlined above.  In one frame there is *no* velocity aberration, while in another frame there *is* velocity aberration.

The solution comes from considering both effects *together*:  The apparent position depends on both the geometric light-time delay and the velocity aberration.  The geometric light-time delay depends only on the target velocity, while the velocity aberration depends only on observer velocity.  **When both effects are computed, they will always cancel out any frame-dependence, yielding a final apparent position that is independent of the inertial frame chosen.**

To illustrate this, lets look at the case of two satellites in orbit around the Earth:
- Both satellites are in roughly the same orbit.  So the observer velocity roughly equals the target velocity (:math:`\vec{v}_o = \vec{v}_t`)
- They are separated by :math:`d = 10\text{km}`.

We'll do the calculation of the apparent position in both an ECI frame (:math:`E`) and an SSB frame (:math:`S`).

In the ECI frame, the positions for both satellites are:

.. math::

    ^E\vec{r}_o = \left[400, 0, 0\right] \text{km}\\ 
    ^E\vec{r}_t = \left[410, 0, 0\right] \text{km}\\

(because offset by :math:`d=10\text{km}`), while their velocities are:


.. math::

    ^E\vec{v}_o \approx \ ^E\vec{v}_t = \left[0, 7, 0\right] \text{km/s}

In the SSB frame, the states are:

.. math::

    ^S\vec{r}_o \approx \ ^S\vec{r}_t = \left[1.5\times10^8, 0, 0\right] \text{km}\\
    ^S\vec{v}_o \approx \ ^S\vec{v}_t = \left[0, 37000, 0\right] \text{km/s}

(transverse velocities are assumed for the "worst case" aberration)


In both ECI and SSB, the light time delay between the spacecraft is:

.. math::

    \delta t = d / c \rightarrow \frac{10\text{km}}{300000\text{km/s}} \approx 33\mu\text{s}


To compute the light time correction (:math:`\delta \vec{r}`), as a first-order approximation (perfectly valid over this time scale), we simply multiply the spacecraft's velocity by $\delta t$.  *This will be different for each frame and only depend on the target velocity!*

.. math::

    ^E\delta \vec{r} \ \approx -^E\vec{v}_t \cdot \delta t \\
    ^S\delta \vec{r} \ \approx -^S\vec{v}_t \cdot \delta t


So to approximate the angular offset caused by the light-time delay, again as a first-order approximation, we can use:

.. math::

    ^E\theta_{LT} \approx \frac{^E\delta \vec{r}}{d}\\
    ^S\theta_{LT} \approx \frac{^S\delta \vec{r}}{d}


What we can notice right off the bat is that :math:`\delta\vec{r} = \vec{v} \cdot \delta t`, and :math:`d = c \cdot \delta t`.  This means we can rewrite the above as:

.. math::

    ^E\theta_{LT} \approx -\frac{|^E\vec{v}_t|\cdot \delta t}{c \cdot \delta t} = -\frac{|^E\vec{v}_t|}{c}\\
    ^S\theta_{LT} \approx -\frac{|^S\vec{v}_t|\cdot \delta t}{c \cdot \delta t} = -\frac{|^S\vec{v}_t|}{c}


Now that we have the angles due to light time correction in each frame, let's compute the angles due to the velocity aberration.

For velocities much smaller than the speed of light, the magnitude of the aberration can be approximated as:

.. math::

    \theta_{ab} = \frac{|\vec{v}_o|}{c}


*Note that this will be different for each frame, and only depends on the observer velocity!*

So when we go to compute the true apparent position (accounting for light-time delay *and* velocity aberration), we see:

.. math::

    \theta_{total} = \theta_{LT} + \theta_{ab}\\

And so computing this for each frame, we get:

.. math::

    ^E\theta_{total} = \frac{|^E\vec{v}_o|}{c} -\frac{|^E\vec{v}_t|}{c}\\
    ^S\theta_{total} = \frac{|^S\vec{v}_o|}{c} -\frac{|^S\vec{v}_t|}{c}\\


Because we assume that these satellites are essentially moving with one another (both around the Earth *and* around the SSB), then we know regardless of interial frame chosen, that:

.. math::
    \vec{v}_o = \vec{v}_t

Therefore we can conclude that the total apparent position is the same in both frames.

So conceptually what happened?  It's obvious as to why the velocity aberration changed between frames:  the observer velocity changed.  The light-time delay is less obvious, since it's easy to think of the delay as only depending on the relative distance.  But in reality, the light-time delay correction depends on where the target *was* when the light left it, which depends on the target velocity.  In the SSB that means how far it moved in it's orbit around the Earth in the few microseconds it took the light to travel.  But in the SSB frame, it also includes how far the Earth (and thus both satellites) moved in their orbit around the Sun.  The light-time delay is therefore *much larger*, allowing it to exactly cancel out the larger velocity aberration.
