
#include "common.h"
#include "NativeCrash.h"

NativeCrash::NativeCrash(AprData* aprData)
{
	m_aprData = aprData;
}

void NativeCrash::init_wd_context(vector<struct wd_context>* pvec)
{
	pvec->push_back(wd_context("/data", IN_CREATE|IN_DELETE|IN_MOVED_FROM));
	pvec->push_back(wd_context("/data/tombstones", IN_CLOSE_WRITE));
}

void NativeCrash::handle_event(int ifd, vector<struct wd_context>* pvec, struct inotify_event* event)
{
	/* /data/tombstones */
	if (event->wd == (*pvec)[1].wd) {
		event_wd1(event);
		return;
	}

	/* /data */
	if (event->wd == (*pvec)[0].wd) {
		event_wd0(ifd, pvec, event);
		return;
	}
}

int NativeCrash::event_wd0(int ifd, vector<wd_context>* pvec, struct inotify_event* event)
{
	int wd;
	if ((event->mask & IN_ISDIR)
		&& !strcmp(event->name, "tombstones")) {

		if (event->mask & IN_CREATE) {
			APR_LOGD("inotify_add_watch(fd, %s)\n", (*pvec)[1].pathname.c_str());
			wd = inotify_add_watch(ifd, (*pvec)[1].pathname.c_str(), (*pvec)[1].mask);
			if (wd < 0) {
				APR_LOGE("Can't add watch for %s.\n", (*pvec)[1].pathname.c_str());
				exit(-1);
			}
			(*pvec)[1].wd = wd;
		} else if (event->mask & IN_MOVED_FROM) {
			APR_LOGD("inotify_rm_watch, %s\n", (*pvec)[1].pathname.c_str());
			wd = inotify_rm_watch(ifd, (*pvec)[1].wd);
			if (wd < 0) {
				APR_LOGE("Can't del watch for %s.\n", (*pvec)[1].pathname.c_str());
				exit(-1);
			}
			(*pvec)[1].wd = -1;
		} else if (event->mask & IN_DELETE) {
			APR_LOGD("inotify_rm_watch, %s\n", (*pvec)[1].pathname.c_str());
			wd = inotify_rm_watch(ifd, (*pvec)[1].wd);
			if (wd < 0) {
				APR_LOGE("Can't rm watch for %s.\n", (*pvec)[1].pathname.c_str());
				if (errno != EINVAL)
					exit(-1);
			}
			(*pvec)[1].wd = -1;
		}
	}

	return 0;
}

int NativeCrash::event_wd1(struct inotify_event* event)
{
	int wd;
	if (!(event->mask & IN_ISDIR)
			&& strstr(event->name, "tombstone_0"))
	{
		if (event->mask & IN_CLOSE_WRITE) {
			// native crash
			m_ei.et = E_NATIVE_CRASH;
			m_aprData->setChanged();
			m_aprData->notifyObservers((void*)&m_ei);
		}
	}

	return 0;
}

