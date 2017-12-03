/*
 * Anope IRC Services
 *
 * Copyright (C) 2012-2017 Anope Team <team@anope.org>
 *
 * This file is part of Anope. Anope is free software; you can
 * redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see see <http://www.gnu.org/licenses/>.
 */

#include "module.h"
#include "modules/sql.h"
#include "modules/nickserv.h"

static Module *me;

class SQLAuthenticationResult : public SQL::Interface
{
	Reference<User> user;
	NickServ::IdentifyRequest *req;

 public:
	SQLAuthenticationResult(User *u, NickServ::IdentifyRequest *r) : SQL::Interface(me), user(u), req(r)
	{
		req->Hold(me);
	}

	~SQLAuthenticationResult()
	{
		req->Release(me);
	}

	void OnResult(const SQL::Result &r) override
	{
		if (r.Rows() == 0)
		{
			this->owner->logger.Debug("m_sql_authentication: Unsuccessful authentication for {0}", req->GetAccount());
			delete this;
			return;
		}

		this->owner->logger.Debug("m_sql_authentication: Successful authentication for {0}", req->GetAccount());

		Anope::string email;
		try
		{
			email = r.Get(0, "email");
		}
		catch (const SQL::Exception &) { }

		NickServ::Nick *na = NickServ::FindNick(req->GetAccount());
		ServiceBot *NickServ = Config->GetClient("NickServ");
		if (na == NULL)
		{
			NickServ::Account *nc = Serialize::New<NickServ::Account *>();
			nc->SetDisplay(req->GetAccount());

			na = Serialize::New<NickServ::Nick *>();
			na->SetNick(nc->GetDisplay());
			na->SetAccount(nc);
			na->SetTimeRegistered(Anope::CurTime);
			na->SetLastSeen(Anope::CurTime);

			this->owner->logger.Log("Created account {0}", nc->GetDisplay());

			EventManager::Get()->Dispatch(&NickServ::Event::NickRegister::OnNickRegister, user, na, "");

			if (user && NickServ)
				user->SendMessage(NickServ, _("Your account \002%s\002 has been successfully created."), na->GetNick().c_str());
		}

		if (!email.empty() && email != na->GetAccount()->GetEmail())
		{
			na->GetAccount()->GetEmail() = email;
			if (user && NickServ)
				user->SendMessage(NickServ, _("Your email has been updated to \002%s\002."), email.c_str());
		}

		req->Success(me);
		delete this;
	}

	void OnError(const SQL::Result &r) override
	{
		this->owner->logger.Log("Error executing query {0}: {1}", r.GetQuery().query, r.GetError());
		delete this;
	}
};

class ModuleSQLAuthentication : public Module
	, public EventHook<Event::PreCommand>
	, public EventHook<Event::CheckAuthentication>
{
	Anope::string engine;
	Anope::string query;
	Anope::string disable_reason, disable_email_reason;

	ServiceReference<SQL::Provider> SQL;

 public:
	ModuleSQLAuthentication(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR)
		, EventHook<Event::PreCommand>(this)
		, EventHook<Event::CheckAuthentication>(this)
	{
		me = this;

	}

	void OnReload(Configuration::Conf *conf) override
	{
		Configuration::Block *config = conf->GetModule(this);
		this->engine = config->Get<Anope::string>("engine");
		this->query =  config->Get<Anope::string>("query");
		this->disable_reason = config->Get<Anope::string>("disable_reason");
		this->disable_email_reason = config->Get<Anope::string>("disable_email_reason");

		this->SQL = ServiceReference<SQL::Provider>(this->engine);
	}

	EventReturn OnPreCommand(CommandSource &source, Command *command, std::vector<Anope::string> &params) override
	{
		if (!this->disable_reason.empty() && (command->GetName() == "nickserv/register" || command->GetName() == "nickserv/group"))
		{
			source.Reply(this->disable_reason);
			return EVENT_STOP;
		}

		if (!this->disable_email_reason.empty() && command->GetName() == "nickserv/set/email")
		{
			source.Reply(this->disable_email_reason);
			return EVENT_STOP;
		}

		return EVENT_CONTINUE;
	}

	void OnCheckAuthentication(User *u, NickServ::IdentifyRequest *req) override
	{
		if (!this->SQL)
		{
			logger.Log("Unable to find SQL engine");
			return;
		}

		SQL::Query q(this->query);
		q.SetValue("a", req->GetAccount());
		q.SetValue("p", req->GetPassword());
		if (u)
		{
			q.SetValue("n", u->nick);
			q.SetValue("i", u->ip.addr());
		}
		else
		{
			q.SetValue("n", "");
			q.SetValue("i", "");
		}


		this->SQL->Run(new SQLAuthenticationResult(u, req), q);

		logger.Debug("Checking authentication for {0}", req->GetAccount());
	}
};

MODULE_INIT(ModuleSQLAuthentication)