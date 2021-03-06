#include "GitRepoLoader.h"

#include <GitBase.h>
#include <RevisionsCache.h>
#include <GitRequestorProcess.h>
#include <GitBranches.h>

#include <QLogger.h>

#include <QDir>

using namespace QLogger;

static const QString GIT_LOG_FORMAT("%m%HX%P%n%cn<%ce>%n%an<%ae>%n%at%n%s%n%b ");

GitRepoLoader::GitRepoLoader(QSharedPointer<GitBase> gitBase, QSharedPointer<RevisionsCache> cache, QObject *parent)
   : QObject(parent)
   , mGitBase(gitBase)
   , mRevCache(std::move(cache))
{
}

bool GitRepoLoader::loadRepository()
{
   if (mLocked)
      QLog_Warning("Git", "Git is currently loading data.");
   else
   {
      if (mGitBase->getWorkingDir().isEmpty())
         QLog_Error("Git", "No working directory set.");
      else
      {
         QLog_Info("Git", "Initializing Git...");

         mRevCache->clear();

         mLocked = true;

         if (configureRepoDirectory())
         {
            mGitBase->updateCurrentBranch();

            requestRevisions();

            QLog_Info("Git", "... Git init finished");

            return true;
         }
         else
            QLog_Error("Git", "The working directory is not a Git repository.");
      }
   }

   return false;
}

bool GitRepoLoader::configureRepoDirectory()
{
   QLog_Debug("Git", "Configuring repository directory.");

   const auto ret = mGitBase->run("git rev-parse --show-cdup");

   if (ret.success)
   {
      QDir d(QString("%1/%2").arg(mGitBase->getWorkingDir(), ret.output.toString().trimmed()));
      mGitBase->setWorkingDir(d.absolutePath());

      return true;
   }

   return false;
}

void GitRepoLoader::loadReferences()
{
   QLog_Debug("Git", "Loading references.");

   const auto ret3 = mGitBase->run("git show-ref -d");

   if (ret3.success)
   {
      auto ret = mGitBase->getLastCommit();

      if (ret.success)
         ret.output = ret.output.toString().trimmed();

      QString prevRefSha;
      const auto curBranchSHA = ret.output.toString();
      const auto referencesList = ret3.output.toString().split('\n', QString::SkipEmptyParts);

      for (const auto &reference : referencesList)
      {
         const auto revSha = reference.left(40);
         const auto refName = reference.mid(41);

         if (!refName.startsWith("refs/tags/") || (refName.startsWith("refs/tags/") && refName.endsWith("^{}")))
         {
            auto localBranches = false;
            References::Type type;
            QString name;

            if (refName.startsWith("refs/tags/"))
            {
               type = References::Type::Tag;
               name = refName.mid(10, reference.length());
               name.remove("^{}");
            }
            else if (refName.startsWith("refs/heads/"))
            {
               type = References::Type::LocalBranch;
               name = refName.mid(11);
               localBranches = true;
            }
            else if (refName.startsWith("refs/remotes/") && !refName.endsWith("HEAD"))
            {
               type = References::Type::RemoteBranches;
               name = refName.mid(13);
            }
            else
               continue;

            mRevCache->insertReference(revSha, type, name);

            if (localBranches)
            {
               const auto git = new GitBranches(mGitBase);
               RevisionsCache::LocalBranchDistances distances;

               const auto distToMaster = git->getDistanceBetweenBranches(true, name);
               auto toMaster = distToMaster.output.toString();

               if (!toMaster.contains("fatal"))
               {
                  toMaster.replace('\n', "");
                  const auto values = toMaster.split('\t');
                  distances.behindMaster = values.first().toUInt();
                  distances.aheadMaster = values.last().toUInt();
               }

               const auto distToOrigin = git->getDistanceBetweenBranches(false, name);
               auto toOrigin = distToOrigin.output.toString();

               if (!toOrigin.contains("fatal"))
               {
                  toOrigin.replace('\n', "");
                  const auto values = toOrigin.split('\t');
                  distances.behindOrigin = values.first().toUInt();
                  distances.aheadOrigin = values.last().toUInt();
               }

               mRevCache->insertLocalBranchDistances(name, distances);
            }
         }

         prevRefSha = revSha;
      }
   }
}

void GitRepoLoader::requestRevisions()
{
   QLog_Debug("Git", "Loading revisions.");

   const auto baseCmd = QString("git log --date-order --no-color --log-size --parents --boundary -z --pretty=format:")
                            .append(GIT_LOG_FORMAT)
                            .append(mShowAll ? QString("--all") : mGitBase->getCurrentBranch());

   const auto requestor = new GitRequestorProcess(mGitBase->getWorkingDir());
   connect(requestor, &GitRequestorProcess::procDataReady, this, &GitRepoLoader::processRevision);
   connect(this, &GitRepoLoader::cancelAllProcesses, requestor, &AGitProcess::onCancel);

   requestor->run(baseCmd);
}

void GitRepoLoader::processRevision(const QByteArray &ba)
{
   QLog_Debug("Git", "Processing revisions...");

   const auto commits = ba.split('\000');
   const auto totalCommits = commits.count();
   auto count = 1;

   QLog_Debug("Git", QString("There are {%1} commits to process.").arg(totalCommits));

   mRevCache->configure(totalCommits);

   emit signalLoadingStarted(totalCommits);

   QLog_Debug("Git", QString("Adding the WIP commit."));

   updateWipRevision();

   for (const auto &commitInfo : commits)
   {
      CommitInfo revision(commitInfo);

      if (revision.isValid())
         mRevCache->insertCommitInfo(std::move(revision), count);
      else
         break;

      emit signalLoadingStep(count++);
   }

   mLocked = false;

   loadReferences();

   emit signalLoadingFinished();
}

void GitRepoLoader::updateWipRevision()
{
   QLog_Debug("Git", QString("Executing updateWipRevision."));

   mRevCache->setUntrackedFilesList(getUntrackedFiles());

   const auto ret = mGitBase->run("git rev-parse --revs-only HEAD");

   if (ret.success)
   {
      const auto parentSha = ret.output.toString().trimmed();

      const auto ret3 = mGitBase->run(QString("git diff-index %1").arg(parentSha));
      const auto diffIndex = ret3.success ? ret3.output.toString() : QString();

      const auto ret4 = mGitBase->run(QString("git diff-index --cached %1").arg(parentSha));
      const auto diffIndexCached = ret4.success ? ret4.output.toString() : QString();

      mRevCache->updateWipCommit(parentSha, diffIndex, diffIndexCached);
   }
}

QVector<QString> GitRepoLoader::getUntrackedFiles() const
{
   QLog_Debug("Git", QString("Executing getUntrackedFiles."));

   auto runCmd = QString("git ls-files --others");
   const auto exFile = QString(".git/info/exclude");
   const auto path = QString("%1/%2").arg(mGitBase->getWorkingDir(), exFile);

   if (QFile::exists(path))
      runCmd.append(QString(" --exclude-from=$%1$").arg(exFile));

   runCmd.append(QString(" --exclude-per-directory=$%1$").arg(".gitignore"));

   return mGitBase->run(runCmd).output.toString().split('\n', QString::SkipEmptyParts).toVector();
}
