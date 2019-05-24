#include "module.h"

/* Constants */
static Anope::string rep_nick = "$nick$";
static Anope::string rep_network = "$network$";

/* Variables */
static Anope::string NetworkHost;
//static int MergeHosts;

class NetHost : public Module
{
  public:
	NetHost(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, THIRD)
	{
		this->SetAuthor("Ashley Lavery");
    this->SetVersion("1.1");

		if (!IRCD || !IRCD->CanSetVHost)
			throw ModuleException("Your IRCd does not support vhosts");

		NetworkHost = Config->GetModule(this)->Get<const Anope::string>("NetworkHost");
	}



	void OnReload(Configuration::Conf *conf) anope_override
	{
		NetworkHost = conf->GetModule(this)->Get<const Anope::string>("NetworkHost");
	}

	void OnNickRegister(User *user, NickAlias *na, const Anope::string &pass) anope_override
	{
		Anope::string cust_host, vIdent, vHost;

		if(NetworkHost.empty())
			return;

		/* Create the customized host for the user. */
		cust_host = NetworkHost.replace_all_cs(rep_nick, na->nick);
		cust_host = cust_host.replace_all_cs(rep_network, Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));

		size_t a = cust_host.find('@');

		if (a == Anope::string::npos)
			vHost = cust_host;
		else
		{
			vIdent = cust_host.substr(0, a);
			vHost = cust_host.substr(a + 1);
		}

		if(!vIdent.empty()) vIdent = vident_valid(vIdent);
		vHost = vhost_valid(vHost);

		if (vHost.empty())
			return;

		if(!vIdent.empty())
		{
			if(vIdent.length() > Config->GetBlock("networkinfo")->Get<unsigned>("userlen"))
			{
				return;
			}
			else if (!IRCD->CanSetVIdent)
			{
				return;
			}
			for (Anope::string::iterator s = vIdent.begin(), s_end = vIdent.end(); s != s_end; ++s)
				if (!isvalidchar(*s))
				{
					return;
				}
		}

		if (vHost.length() > Config->GetBlock("networkinfo")->Get<unsigned>("hostlen"))
		{
			return;
		}

		if (!IRCD->IsHostValid(vHost))
		{
			return;
		}

		Log() << "[hs_nethost] Automatically setting new vhost for nick " << na->nick << " " << (!vIdent.empty() ? vIdent + "@" : "") << vHost << ".";
		na->SetVhost(vIdent, vHost, "HostServ", Anope::CurTime);
		FOREACH_MOD(OnSetVhost, (na));
	}

  private:
	Anope::string vident_valid(Anope::string ident)
	{
		Anope::string new_ident;

		for(Anope::string::iterator s = ident.begin(), s_end = ident.end(); s != s_end; ++s)
		{
			if(!isvalidchar(*s))
			{
				new_ident = new_ident + '-';
			}
			else
				new_ident = new_ident + *s;
		}

		if(new_ident.length() > Config->GetBlock("networkinfo")->Get<unsigned>("userlen"))
		{
			new_ident = new_ident.substr(0, Config->GetBlock("networkinfo")->Get<unsigned>("userlen") - 1);
		}

		return new_ident;
	}

	Anope::string vhost_valid(Anope::string host)
	{
		Anope::string new_host;
		int ok = 0;

		for(Anope::string::iterator s = host.begin(), s_end = host.end(); s != s_end; ++s)
		{
			if(!isvalidchar(*s) && *s != '-')
			{
				if(!ok)
					new_host = new_host + 'x';
				else
					new_host = new_host + '.';
			}
			else
			{
				new_host = new_host + *s;
				ok = 1;
			}
		}

		return new_host;
	}

	bool isvalidchar(char c)
	{
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-')
			return true;
		return false;
	}
};

MODULE_INIT(NetHost)
