/* This file is part of 3hs
 * Copyright (C) 2021-2022 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "hlink/templ.hh"

#include <string.h>

using hlink::TemplArgs;
using hlink::TemplStrFunc;
using hlink::TemplBoolFunc;
using hlink::TemplCtx;

static bool b_not_impl(TemplCtx& ctx, TemplArgs& args);
static bool b_eq_impl(TemplCtx& ctx, const TemplArgs& args)
{
	if(args.size() < 2)
	{
		ctx.abort();
		return false;
	}

	return args[0] == args[1];
}

static std::string abort_impl(hlink::TemplCtx& ctx, const TemplArgs&)
{
	ctx.abort();
	return "";
}

static std::string xref_impl(hlink::TemplCtx& ctx, const TemplArgs& args)
{
	if(args.size() != 3)
		return ctx.abort(), "";

	if(ctx.ren->syms.count(args[1]) == 0 || ctx.ren->syms.count(args[2]))
		return ctx.abort(), "";

	hlink::TemplRen::templ_sym& dst = ctx.ren->syms[args[1]];
	hlink::TemplRen::templ_sym& haystack = ctx.ren->syms[args[1]];
	if((dst.type != haystack.type) || dst.type != hlink::TemplRen::templ_sym_type::vs)
		return ctx.abort(), "";

	const std::string& needle = args[0];
	size_t smallest = dst.vs->size() > haystack.vs->size()
		? haystack.vs->size() : dst.vs->size();
	for(size_t i = 0; i < smallest; ++i)
	{
		if((*haystack.vs)[i] == needle)
			return (*dst.vs)[i];
	}

	return "";
}

static std::string get_user_agent(hlink::TemplRen *ren)
{
	if(ren->headers().count("user-agent") == 0)
		return "";
	return ren->headers()["user-agent"];
}

void hlink::TemplCtx::abort() { this->ren->abortBit = true; }

static void del_sym(hlink::TemplRen::templ_sym sym)
{
	switch(sym.type)
	{
	case hlink::TemplRen::templ_sym_type::vs:
		delete sym.vs;
		break;
	case hlink::TemplRen::templ_sym_type::ss:
		delete sym.ss;
		break;
	case hlink::TemplRen::templ_sym_type::sf:
		delete sym.sf;
		break;
	case hlink::TemplRen::templ_sym_type::bf:
		delete sym.bf;
		break;
	}
}

hlink::TemplRen::~TemplRen()
{
	for(std::unordered_map<std::string, hlink::TemplRen::templ_sym>::iterator it = this->syms.begin();
		it != this->syms.end(); ++it)
	{
		del_sym(it->second);
	}
}

void hlink::TemplRen::use_default()
{
	this->use("user-agent", get_user_agent(this));
	this->use("abort()", abort_impl);
	this->use("not?()", b_not_impl);
	this->use("xref()", xref_impl);
	this->use("eq?()", b_eq_impl);
}

void hlink::TemplRen::use(const std::string& sym, const std::vector<std::string>& val)
{
	hlink::TemplRen::templ_sym rsym;
	rsym.type = hlink::TemplRen::templ_sym_type::vs;
	rsym.vs = new std::vector<std::string>(val);
	if(this->syms.count(sym) != 0)
		delete this->syms[sym].vs;
	this->syms[sym] = rsym;
}

void hlink::TemplRen::use(const std::string& sym, const std::string& val)
{
	hlink::TemplRen::templ_sym rsym;
	rsym.type = hlink::TemplRen::templ_sym_type::ss;
	rsym.ss = new std::string(val);
	if(this->syms.count(sym) != 0)
		delete this->syms[sym].ss;
	this->syms[sym] = rsym;
}

void hlink::TemplRen::use(const std::string& sym, TemplBoolFunc func)
{
	hlink::TemplRen::templ_sym rsym;
	rsym.type = hlink::TemplRen::templ_sym_type::bf;
	rsym.bf = new TemplBoolFunc(func);
	if(this->syms.count(sym) != 0)
		delete this->syms[sym].bf;
	this->syms[sym] = rsym;
}

void hlink::TemplRen::use(const std::string& sym, TemplStrFunc func)
{
	hlink::TemplRen::templ_sym rsym;
	rsym.type = hlink::TemplRen::templ_sym_type::sf;
	rsym.sf = new TemplStrFunc(func);
	if(this->syms.count(sym) != 0)
		delete this->syms[sym].sf;
	this->syms[sym] = rsym;
}

std::string hlink::TemplRen::strerr(hlink::TemplRen::result res)
{
	switch(res)
	{
	case hlink::TemplRen::result::ok:
		return "OK";
	case hlink::TemplRen::result::aborted:
		return "Aborted";
	case hlink::TemplRen::result::unterminated:
		return "Unterminated";
	case hlink::TemplRen::result::not_found:
		return "Symbol not found";
	case hlink::TemplRen::result::invalid:
		return "Invalid";
	}
	return "Unknown";
}

using SplitRes = std::vector<std::string>;

static bool eval_boolean(TemplCtx& ctx, SplitRes& args)
{
	if(args.size() == 0) return false;
	std::string ssym = args[0];
	args.erase(args.begin());

	if(ctx.ren->syms.count(ssym) == 0) return false;
	hlink::TemplRen::templ_sym& sym = ctx.ren->syms[ssym];

	if(sym.type != hlink::TemplRen::templ_sym_type::bf)
		return false;

	return (*sym.bf)(ctx, args);
}

static bool b_not_impl(TemplCtx& ctx, TemplArgs& args)
{ return !eval_boolean(ctx, args); }

static hlink::TemplRen::result eval_string(TemplCtx& ctx, std::string& res, SplitRes& args)
{
	if(args.size() == 0) return hlink::TemplRen::result::not_found;
	std::string ssym = args[0];
	args.erase(args.begin());

	if(ctx.ren->syms.count(ssym) == 0) return hlink::TemplRen::result::not_found;
	hlink::TemplRen::templ_sym& sym = ctx.ren->syms[ssym];

	switch(sym.type)
	{
	case hlink::TemplRen::templ_sym_type::vs:
		return hlink::TemplRen::result::invalid;
	case hlink::TemplRen::templ_sym_type::ss:
		res = *sym.ss;
		break;
	case hlink::TemplRen::templ_sym_type::sf:
		res = (*sym.sf)(ctx, args);
		break;
	case hlink::TemplRen::templ_sym_type::bf:
		res = (*sym.bf)(ctx, args) ? "true" : "false";
		break;
	}

	return hlink::TemplRen::result::ok;
}

/* returns true if the split failed due to an unterminated quote */
static bool split_until_bracket(const std::string& src, SplitRes& res, size_t& i)
{
	bool inQuote = false;
	std::string cur;

	for(; i < src.size(); ++i)
	{
		/* escaped character */
		if(src[i] == '\\')
		{
			cur.push_back(src[++i]);
			continue;
		}

		else if(src[i] == '\'')
		{
			inQuote = !inQuote;
			continue;
		}

		else if(!inQuote)
		{
			/* split opportunity */
			if(src[i] == ' ')
			{
				res.push_back(cur);
				cur.clear();
				/* skip all whitespace */
				while(src[i] == ' ')
					++i;
				--i;
				continue;
			}

			else if(src[i] == ']') break; /* done */
		}

		cur.push_back(src[i]);
	}

	if(cur != "") res.push_back(cur);
	return inQuote;
}

static void skip_to_keywords(const std::string& src, size_t& i, const char *keywords[], size_t klen)
{
	for(; i < src.size(); ++i)
	{
		if(src[i] == '\\') ++i;
		else if(strncmp(src.c_str() + i, "[[", 2) == 0)
		{
			i += 2;
			for(size_t j = 0; j < klen; ++j)
			{
				size_t kwlen = strlen(keywords[j]);
				if(strncmp(src.c_str() + i, keywords[j], kwlen) == 0)
				{
					i -= 3; // because the caller will consume the c in c[[<kw>]]
					return;
				}
			}
		}
	}
}

void hlink::TemplRen::skip_to_next_if_like_block(const std::string& src, size_t& i)
{
	const char *keywords[] = { "end", "if", "else-if", "else" };
	skip_to_keywords(src, i, keywords, sizeof(keywords) / sizeof(keywords[0]));
}

hlink::TemplRen::result hlink::TemplRen::finish_until_end_kw(const std::string& src, std::string& res, hlink::TemplCtx& ctx, size_t& i)
{
	const char *keywords[] = { "end" };
	return this->finish_until_keyword(src, res, ctx, i,
		keywords, sizeof(keywords) / sizeof(keywords[0]));
}

hlink::TemplRen::result hlink::TemplRen::finish_until_keyword(const std::string& src, std::string& res, hlink::TemplCtx& ctx, size_t& i,
	const char *keywords[], size_t klen)
{
	hlink::TemplRen::result code;
	for(; i < src.size(); ++i)
	{
		/* escaped character */
		if(src[i] == '\\')
		{
			res.push_back(src[++i]);
			continue;
		}

		/* formatting character */
		else if(src[i] == '[')
		{
			++i;
			if(i + 1 == src.size()) return hlink::TemplRen::result::unterminated;
			if(src[i] == '[') /* operator */
			{
				++i;
				if(i + 1 == src.size()) return hlink::TemplRen::result::unterminated;
				SplitRes args;
				if(split_until_bracket(src, args, i))
					return hlink::TemplRen::result::unterminated;
				i += 2; /* skip the next ]] */

				std::string op = args[0];
				args.erase(args.begin());

				for(size_t j = 0; j < klen; ++j)
				{
					if(strcmp(op.c_str(), keywords[j]) == 0)
					{
						--i; /* next iteration in the caller will finish the last ] */
						return hlink::TemplRen::result::ok;
					}
				}

				/* we get to evaluate an if-statement */
				if(op == "if" || op == "else-if")
				{
					if(eval_boolean(ctx, args))
					{
						const char *keywords[] = { "else", "else-if", "end" };
						if((code = this->finish_until_keyword(src, res, ctx, i, keywords, 3))
							!= hlink::TemplRen::result::ok)
							return code;
						const char end[] = "[[end]]";
						/* if the ending keyword was [[else]] or [[else-if]] skip to the next [[end]]
						 * kinda hacky but it works well enough for now.
						 * FIXME: Consider other [[if]]'s and associated [[end]]'s in
						 *        the discarded [[else]] (or [[else-if]]) block */
						if(strcmp(src.c_str() + i - sizeof(end) + 1, end) != 0)
						{
							const char *keywords[] = { "end" };
							skip_to_keywords(src, i, keywords, 1);
						}
					}
					else
					{
						this->skip_to_next_if_like_block(src, i);
					}
				}

				/* always true; if it wasn't we would've skipped over it before */
				else if(op == "else")
				{
					if((code = this->finish_until_end_kw(src, res, ctx, i)) != hlink::TemplRen::result::ok)
						return code;
				}

				else if(op == "foreach")
				{
					/* [[foreach <symnam> in <sym>]] */
					if(args.size() != 3) return hlink::TemplRen::result::invalid;
					if(args[1] != "in") return hlink::TemplRen::result::invalid;
					std::string symname = args[0];
					if(this->syms.count(args[2]) == 0)
						return hlink::TemplRen::result::not_found;
					hlink::TemplRen::templ_sym sym = this->syms[args[2]];
					if(sym.type != hlink::TemplRen::templ_sym_type::vs)
						return hlink::TemplRen::result::invalid;
					size_t ci = i; /* used for resetting */
					for(const std::string& val : *sym.vs)
					{
						i = ci;
						this->use(symname, val);
						if((code = this->finish_until_end_kw(src, res, ctx, i)) != hlink::TemplRen::result::ok)
							return code;
					}
					/* i does *not* get set to the appropriate [[end]] */
					if(sym.vs->size() == 0)
					{
						const char *kws[] = { "end" };
						skip_to_keywords(src, i, kws, 1);
					}
				}

				else return hlink::TemplRen::result::invalid;
			}
			else
			{
				SplitRes args;
				if(split_until_bracket(src, args, i))
					return hlink::TemplRen::result::unterminated;
				std::string symr;
				if((code = eval_string(ctx, symr, args)) != hlink::TemplRen::result::ok)
					return code;
				if(this->abortBit) return hlink::TemplRen::result::aborted;
				res += symr;
			}
			continue;
		}

		/* normal character */
		res.push_back(src[i]);
	}

	return hlink::TemplRen::result::ok;
}

hlink::TemplRen::result hlink::TemplRen::finish(const std::string& src, std::string& res)
{
	this->abortBit = false;
	hlink::TemplCtx ctx;
	ctx.ren = this;

	res.reserve(src.size());
	size_t i = 0;

	return this->finish_until_end_kw(src, res, ctx, i);
}

