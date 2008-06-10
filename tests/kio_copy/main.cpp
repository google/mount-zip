//
// Contributed by Alex Custov (nickname alex_custov at linux.org.ru)
//
// License: GPL
//

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h> 
#include <kio/job.h>
#include <kurl.h>

#include <cstdlib>

static KCmdLineOptions options[] =
{
    {"+[URL to copy from]", I18N_NOOP("URL to copy from."), 0},
    {"+[URL to copy to]",   I18N_NOOP("URL to copy to."),   0},
    { 0, 0, 0 }
};

int main(int argc, char **argv)
{
    KAboutData aboutData("kiocopy", "KIOCopy", "0.1.0", "KIOCopy",
            KAboutData::License_GPL, "(c) 2008, Me",
            QString::null, "http://bgg.net ", QString::null);
KCmdLineArgs::init(argc, argv, &aboutData);
    KCmdLineArgs::addCmdLineOptions(options);
    KCmdLineArgs *kargs = KCmdLineArgs::parsedArgs();
    KURL from, to; 
    KApplication a;

    if(kargs->count() == 2)
    {
        from = KURL::fromPathOrURL(kargs->arg(0));
        to = KURL::fromPathOrURL(kargs->arg(1));
    }
    else
        exit(1);

    KIO::Job *job = KIO::copy(from, to, false);

    QObject::connect(job, SIGNAL(result(KIO::Job *)), &a, SLOT(quit()));

    return a.exec();
}
