#
# This file is the default set of rules to compile a Pebble application.
#
# Feel free to customize this to your needs.
#
import os.path

top = '.'
out = 'build'


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    """
    This method is used to configure your build. ctx.load(`pebble_sdk`) automatically configures
    a build for each valid platform in `targetPlatforms`. Platform-specific configuration: add your
    change after calling ctx.load('pebble_sdk') and make sure to set the correct environment first.
    Universal configuration: add your change prior to calling ctx.load('pebble_sdk').
    """
    ctx.load('pebble_sdk')


def build(ctx):
    ctx.load('pebble_sdk')

    build_worker = os.path.exists('worker_src')
    binaries = []

    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        # DIE APP UND DER WORKER TEILEN SICH DEN KERN in src/c/kern: die
        # Messung lebt im Worker, die App braucht davon nur die Arten und
        # die wartende Zusammenfassung. Jeder bekommt, was er braucht, und
        # der Worker obendrein -DKS_WORKER, damit plattform.h ihm
        # pebble_worker.h statt pebble.h gibt.
        app_quellen = (ctx.path.ant_glob('src/c/*.c') +
                       ctx.path.ant_glob('src/c/kern/art.c') +
                       ctx.path.ant_glob('src/c/kern/wartend.c') +
                       ctx.path.ant_glob('src/c/kern/kurve.c'))
        ctx.pbl_build(source=app_quellen, target=app_elf, bin_type='app',
                      includes=['src/c/kern'])

        if build_worker:
            worker_elf = '{}/pebble-worker.elf'.format(ctx.env.BUILD_DIR)
            binaries.append({'platform': platform, 'app_elf': app_elf, 'worker_elf': worker_elf})
            ctx.pbl_build(source=(ctx.path.ant_glob('worker_src/c/**/*.c') +
                                  ctx.path.ant_glob('src/c/kern/*.c')),
                          target=worker_elf,
                          bin_type='worker',
                          includes=['src/c/kern'],
                          cflags=['-DKS_WORKER'])
        else:
            binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                         'src/pkjs/**/*.json',
                                         'src/common/**/*.js']),
                   js_entry_file='src/pkjs/index.js')
