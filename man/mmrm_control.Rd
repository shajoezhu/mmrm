% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/fit.R
\name{mmrm_control}
\alias{mmrm_control}
\title{Control Parameters for Fitting an MMRM}
\usage{
mmrm_control(
  optimizer = stats::nlminb,
  optimizer_args = list(),
  optimizer_control = list(),
  start = NULL,
  accept_singular = TRUE
)
}
\arguments{
\item{optimizer}{(\code{function})\cr optimization function.}

\item{optimizer_args}{(\code{list})\cr additional arguments to be passed to optimizer.}

\item{optimizer_control}{(\code{list})\cr specific \code{control} argument for optimizer.}

\item{start}{(\code{numeric} or \code{NULL})\cr optional start values for variance
parameters.}

\item{accept_singular}{(\code{flag})\cr whether singular design matrices are reduced
to full rank automatically and additional coefficient estimates will be missing.}
}
\value{
List of class \code{mmrm_control} with the control parameters.
}
\description{
\ifelse{html}{\href{https://lifecycle.r-lib.org/articles/stages.html#experimental}{\figure{lifecycle-experimental.svg}{options: alt='[Experimental]'}}}{\strong{[Experimental]}}
}
\examples{
mmrm_control(
  optimizer = stats::optim,
  optimizer_args = list(method = "L-BFGS-B")
)
}
