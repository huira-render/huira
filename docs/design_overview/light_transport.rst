============================================
Light Transport Model & Time-Delay Precision
============================================

This document details the relativistic light transport assumptions used by the rendering engine. 
Specifically, it addresses the handling of Light Time Delay (LTD) and Aberration for light sources 
relative to the Camera versus the Scene Geometry.

.. contents:: Table of Contents
   :depth: 2
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
========

The engine utilizes a **Camera-Centric Light Transport** model. 

While geometric objects (planets, spacecraft) are rigorously solved for Light Time Delay and 
Aberration relative to the observer, **Light Sources** are approximated as being viewed 
directly by the Camera, rather than by the surfaces they illuminate.

This approximation significantly improves performance (O(N) vs O(N*M)) while maintaining 
sub-pixel accuracy for the vast majority of orbital and astrodynamic scenarios.

The Approximation
=================

In a fully rigorous relativistic renderer, the position of a light source :math:`P_{light}` 
should be solved relative to the surface point :math:`P_{surf}` being shaded. This accounts 
for the specific time it took light to travel from the source to the surface, and then to 
the camera (the "Two-Leg" path).

Instead, this engine computes :math:`P_{light}` relative to the **Camera** at the time of 
observation.

.. image:: /_static/diagrams/light_triangle.svg
   :align: center
   :alt: Diagram showing the triangle formed by Camera, Surface, and Light Source, highlighting the difference between the Camera-Centric path and the Surface-Centric path.

Mathematical Formulation
------------------------

Let :math:`t_{cam}` be the simulation time at the camera.
Let :math:`\tau_{A \to B}` be the light time delay between points A and B.

**1. Rigorous Model (Surface-Centric):**
The surface sees the light at a time earlier than the camera sees the surface.

.. math::

   t_{surf} = t_{cam} - \tau_{surf \to cam}
   
   P_{light}^{rigorous} = P_{light}(t_{surf} - \tau_{light \to surf})

**2. Engine Model (Camera-Centric):**
We assume the light position is sampled at the time it would reach the camera directly.

.. math::

   P_{light}^{engine} = P_{light}(t_{cam} - \tau_{light \to cam})

This introduces a temporal discrepancy :math:`\Delta t`:

.. math::

   \Delta t = (t_{cam} - \tau_{light \to cam}) - (t_{cam} - \tau_{surf \to cam} - \tau_{light \to surf})

If the light source has a velocity :math:`\vec{V}_{light}`, this results in a positional error:

.. math::

   \vec{E}_{pos} \approx \vec{V}_{light} \cdot \Delta t

Validity & Constraints
======================

This approximation is valid for 99% of Solar System rendering tasks. The error :math:`\vec{E}_{pos}` 
approaches zero in the following standard cases:

Case 1: The "Sun" (Static Frame)
--------------------------------
When the light source is the Sun (or the Solar System Barycenter), the velocity 
:math:`\vec{V}_{light}` relative to the inertial frame is effectively zero. Even if :math:`\Delta t` 
is large (e.g., minutes), the Sun's position does not change significantly during that interval.

Case 2: Attached Lights (Co-Moving)
-----------------------------------
When a light source is attached to a vehicle (e.g., a docking spotlight on a robotic arm), 
the light and the illuminated mesh share the same inertial frame.
The relative velocity is zero, and thus the positional error is zero.

.. image:: /_static/diagrams/local_light.svg
   :align: center
   :alt: Diagram showing a spacecraft with an attached spotlight, illustrating that the light moves with the mesh, nullifying the error.

Failure Cases
=============

The approximation breaks down in **"Space Battle"** scenarios involving high-velocity, 
disconnected light sources.

**Scenario:** A relativistic interceptor (Light Source) flying at 0.5c illuminates a static moon 
while passing it.

1. **The Discrepancy:** The Camera calculates the interceptor's position based on a direct line of sight.
2. **The Reality:** The Moon surface "saw" the interceptor at a different time (due to the geometry of the triangle).
3. **The Artifact:** Shadows cast by craters may point in directions that disagree with the rendered position of the interceptor.

.. image:: /_static/diagrams/light_failure_case.svg
   :align: center
   :alt: Diagram showing a high-velocity independent light source creating a 'Ghost Light' artifact where shadows disagree with the visible position of the source.

Mitigation
----------
For scientific analysis of such scenarios (e.g., Light Echoes, Relativistic Flybys), users must 
manually account for this limitation or request a custom "Surface-Centric" integrator.
