/* === S Y N F I G ========================================================= */
/*!	\file synfig/rendering/renderer.cpp
**	\brief Renderer
**
**	$Id$
**
**	\legal
**	......... ... 2015 Ivan Mahonin
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

#include "renderer.h"

#include "../general.h"

#include "software/renderersw.h"
#include "opengl/renderergl.h"

#endif

using namespace synfig;
using namespace rendering;

/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */

Renderer::Handle Renderer::blank;
std::map<String, Renderer::Handle> *Renderer::renderers;

Renderer::~Renderer() { }

bool
Renderer::is_optimizer_registered(const Optimizer::Handle &optimizer) const
{
	for(Optimizer::List::const_iterator i = optimizers.begin(); i != optimizers.end(); ++i)
		if (*i == optimizer) return true;
	return false;
}

void
Renderer::register_optimizer(const Optimizer::Handle &optimizer)
{
	if (optimizer) {
		assert(!is_optimizer_registered(optimizer));
		optimizers.push_back(optimizer);
	}
}

void
Renderer::unregister_optimizer(const Optimizer::Handle &optimizer)
{
	for(Optimizer::List::iterator i = optimizers.begin(); i != optimizers.end();)
		if (*i == optimizer) i = optimizers.erase(i); else ++i;
}

bool
Renderer::optimize_recursive(const Optimizer &optimizer, const Optimizer::RunParams& params) const
{
	if (params.task) {
		for(Task::List::const_iterator i = params.task->sub_tasks.begin(); i != params.task->sub_tasks.end(); ++i)
		{
			if (*i)
			{
				Optimizer::RunParams sub_params(params);
				sub_params.task = *i;
				sub_params.out_task.reset();

				if ( optimizer.run(sub_params)
				  || optimize_recursive(optimizer, sub_params) )
				{
					Task::Handle new_task = params.task->clone();
					new_task->sub_tasks[ i - params.task->sub_tasks.begin() ]
						= sub_params.out_task;
					return true;
				}
			}
		}
	}
	return false;
}

bool
Renderer::optimize_recursive(const Optimizer &optimizer, Task::List &list) const
{
	Optimizer::RunParams params(*this, list);
	for(Task::List::iterator i = list.begin(); i != list.end(); ++i)
	{
		params.task = *i;
		params.out_task.reset();
		if (optimize_recursive(optimizer, params))
		{
			if (params.out_task) *i = params.out_task; else list.erase(i);
			return true;
		}
	}
	return false;
}

void
Renderer::optimize(Task::List &list) const
{
	// remove nulls
	for(Task::List::iterator i = list.begin(); i != list.end();)
		if (*i) ++i; else i = list.erase(i);

	// optimize
	for(Optimizer::List::const_iterator i = optimizers.begin(); i != optimizers.end();) {
		Optimizer::RunParams params(*this, list);
		if ( (*i)->run(params)
		  || optimize_recursive(**i, list) )
			{ i = optimizers.begin(); continue; }
	 }
}

bool
Renderer::run(const Task::List &list) const
{
	Task::List optimized_list(list);
	optimize(optimized_list);

	bool success = true;

	Task::RunParams params;
	for(Task::List::const_iterator i = optimized_list.begin(); i != optimized_list.end(); ++i)
		if (!(*i)->run(params)) success = false;

	return success;
}

void
Renderer::initialize()
{
	if (renderers != NULL)
		synfig::error("rendering::Renderer already initialized");
	renderers = new std::map<String, Handle>();

	// initialize renderers
	register_renderer("software", new RendererSW());
	register_renderer("gl", new RendererGL());
}

void
Renderer::deinitialize()
{
	while(!get_renderers().empty())
		unregister_renderer(get_renderers().begin()->first);
	delete renderers;
}

void
Renderer::register_renderer(const String &name, const Renderer::Handle &renderer)
{
	if (get_renderer(name))
		synfig::error("rendering::Renderer renderer '%s' already registered", name.c_str());
	(*renderers)[name] = renderer;
}

void
Renderer::unregister_renderer(const String &name)
{
	if (!get_renderer(name))
		synfig::error("rendering::Renderer renderer '%s' not registered", name.c_str());
	renderers->erase(name);
}

const Renderer::Handle&
Renderer::get_renderer(const String &name)
{
	return get_renderers().count(name) > 0
		 ? get_renderers().find(name)->second
		 : blank;
}

const std::map<String, Renderer::Handle>&
Renderer::get_renderers()
{
	if (renderers == NULL)
		synfig::error("rendering::Renderer not initialized");
	return *renderers;
}

/* === E N T R Y P O I N T ================================================= */