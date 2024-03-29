+++
title = "A Newfound Love for Jekyll"
publishdate = 2018-04-23T13:49:00-07:00
lastmod = 2022-07-17T01:17:00-07:00
aliases = [
    "/blog/2018/4/23/on-jekyll/",
    "/blog/posts/on-jekyll/"
]
summary = "In which I realize the utility of static site generators."
tags = ["jekyll", "development"]
toc = false
+++
{{% update %}}
I've been using Hugo instead of Jekyll for several years.
I find that it's more flexible.
{{% /update %}}
When I first made my website back in 2016, I decided to build everything from the ground up.

I wanted to fully understand what steps needed to be taken in order to build a responsive website.
While I've had some formal web design education, most of my web experience has been on ASP.NET and server-side logic.
As a result, creating my own site provided a valuable opportunity to improve my understanding of HTML, CSS, and JavaScript.
However, I didn't take the next step of employing modern web build processes or technologies.

In the interest of putting something on my site, I decided I wanted to try making a blog.
Now, for web developers who actually know what they're doing, this is super simple.
Nowadays there are a multitude of ways to generate pages from templates such that new posts require little more than actually writing the new text.
I unfortunately didn't have any idea how to use any of those technologies, and I (incorrectly) assumed that transitioning my site to incorporate one of them would be an enormous amount of work.
In practice, this led to me manually copying and pasting markup as well as writing new posts in the HTML itself.
In hindsight, this may have been the leading contributor to my general animosity toward web development and also the lack of content on my blog.

This brings us to today. I had heard of [Jekyll][Jekyll] when I was designing my site two years ago, but I didn't have a deep enough understanding of the process in order to fully appreciate how powerful its processing was.
Here's a few of the features that I think are fantastic:
- Writing posts in Markdown is a breeze! Goodbye manually inserting `&nbsp;` everywhere I needed a space next to a tag. Plus, you can embed HTML within your Markdown if absolutely necessary.
- Jekyll allows for the creation of HTML templates (with parameters) that can be reused anywhere in your site.
- Jekyll validates internal links, and won't even build your site if they're invalid.

The key takeaway from using Jekyll is that it allows me to abstract away the hassle of writing markup from writing the page content itself.
Pages can be modularized, allowing, for example, the navigation bar, footer, and central page content to be designed in pieces.
Instead of copying the same markup to every page that needs it, you can create a layout and simply set up the relevant pages to use the layout with a single line.
For instance, the only text in the file for this post other than the words you're reading is:
```yaml
---
layout: post
title:  "A Newfound Love for Jekyll"
subtitle: "Making the Front-End Bearable"
date:   2018-4-23
---
```
Jekyll doesn't remove the need for writing HTML and CSS, but it vastly reduces the amount of work I have to do as the author of a static site. For that reason, I'm a fan.

[Jekyll]: https://jekyllrb.com