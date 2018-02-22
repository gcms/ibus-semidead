/* vim:set et sts=4: */
#ifndef __ENGINE_H__
#define __ENGINE_H__

#include <ibus.h>

#define IBUS_TYPE_SEMIDEAD_ENGINE	\
	(ibus_semidead_engine_get_type ())

GType   ibus_semidead_engine_get_type    (void);

#endif
