KPL/FK

   HUIRA_EARTH_MOD   Mean equator and equinox of date (IAU 1976 precession)
   HUIRA_EARTH_TOD   True equator and equinox of date (IAU 1976 + IAU 1980)
   HUIRA_EARTH_TEME  True equator, mean equinox of date (SGP4 output frame)

   HUIRA_EARTH_TEME is a TWO-VECTOR frame:
      +Z = +Z of HUIRA_EARTH_TOD (true pole of date)
      +X = +X of HUIRA_EARTH_MOD (mean equinox of date), projected onto the
           true equator

   Reproduces Vallado's teme2eci (P76^T N80^T R3(-eqeq), eqeq = dpsi*cos(eps),
   1982 form) to < 0.2 mas. No other kernels required.

   Recursion limit: no other dynamic frame may reference HUIRA_EARTH_TEME, and it
   must not be the base frame of SPK/CK segments.

   Set FRAME_1489003_ROTATION_STATE = 'INERTIAL' to match Vallado's
   teme2eci velocity convention exactly (difference << 1 mm/s).

\begindata

   FRAME_HUIRA_EARTH_MOD          = 1489001
   FRAME_1489001_NAME             = 'HUIRA_EARTH_MOD'
   FRAME_1489001_CLASS            = 5
   FRAME_1489001_CLASS_ID         = 1489001
   FRAME_1489001_CENTER           = 399
   FRAME_1489001_RELATIVE         = 'J2000'
   FRAME_1489001_DEF_STYLE        = 'PARAMETERIZED'
   FRAME_1489001_FAMILY           = 'MEAN_EQUATOR_AND_EQUINOX_OF_DATE'
   FRAME_1489001_PREC_MODEL       = 'EARTH_IAU_1976'
   FRAME_1489001_ROTATION_STATE   = 'ROTATING'

   FRAME_HUIRA_EARTH_TOD          = 1489002
   FRAME_1489002_NAME             = 'HUIRA_EARTH_TOD'
   FRAME_1489002_CLASS            = 5
   FRAME_1489002_CLASS_ID         = 1489002
   FRAME_1489002_CENTER           = 399
   FRAME_1489002_RELATIVE         = 'J2000'
   FRAME_1489002_DEF_STYLE        = 'PARAMETERIZED'
   FRAME_1489002_FAMILY           = 'TRUE_EQUATOR_AND_EQUINOX_OF_DATE'
   FRAME_1489002_PREC_MODEL       = 'EARTH_IAU_1976'
   FRAME_1489002_NUT_MODEL        = 'EARTH_IAU_1980'
   FRAME_1489002_ROTATION_STATE   = 'ROTATING'

   FRAME_HUIRA_EARTH_TEME         = 1489003
   FRAME_1489003_NAME             = 'HUIRA_EARTH_TEME'
   FRAME_1489003_CLASS            = 5
   FRAME_1489003_CLASS_ID         = 1489003
   FRAME_1489003_CENTER           = 399
   FRAME_1489003_RELATIVE         = 'J2000'
   FRAME_1489003_DEF_STYLE        = 'PARAMETERIZED'
   FRAME_1489003_FAMILY           = 'TWO-VECTOR'
   FRAME_1489003_PRI_AXIS         = 'Z'
   FRAME_1489003_PRI_VECTOR_DEF   = 'CONSTANT'
   FRAME_1489003_PRI_FRAME        = 'HUIRA_EARTH_TOD'
   FRAME_1489003_PRI_SPEC         = 'RECTANGULAR'
   FRAME_1489003_PRI_VECTOR       = ( 0.0, 0.0, 1.0 )
   FRAME_1489003_SEC_AXIS         = 'X'
   FRAME_1489003_SEC_VECTOR_DEF   = 'CONSTANT'
   FRAME_1489003_SEC_FRAME        = 'HUIRA_EARTH_MOD'
   FRAME_1489003_SEC_SPEC         = 'RECTANGULAR'
   FRAME_1489003_SEC_VECTOR       = ( 1.0, 0.0, 0.0 )

\begintext